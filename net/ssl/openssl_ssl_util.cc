// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_ssl_util.h"

#include <errno.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/values.h"
#include "crypto/openssl_util.h"
#include "net/base/net_errors.h"

namespace net {

SslSetClearMask::SslSetClearMask()
    : set_mask(0),
      clear_mask(0) {
}

void SslSetClearMask::ConfigureFlag(long flag, bool state) {
  (state ? set_mask : clear_mask) |= flag;
  // Make sure we haven't got any intersection in the set & clear options.
  DCHECK_EQ(0, set_mask & clear_mask) << flag << ":" << state;
}

namespace {

class OpenSSLNetErrorLibSingleton {
 public:
  OpenSSLNetErrorLibSingleton() {
    crypto::EnsureOpenSSLInit();

    // Allocate a new error library value for inserting net errors into
    // OpenSSL. This does not register any ERR_STRING_DATA for the errors, so
    // stringifying error codes through OpenSSL will return NULL.
    net_error_lib_ = ERR_get_next_error_library();
  }

  unsigned net_error_lib() const { return net_error_lib_; }

 private:
  unsigned net_error_lib_;
};

base::LazyInstance<OpenSSLNetErrorLibSingleton>::Leaky g_openssl_net_error_lib =
    LAZY_INSTANCE_INITIALIZER;

unsigned OpenSSLNetErrorLib() {
  return g_openssl_net_error_lib.Get().net_error_lib();
}

int MapOpenSSLErrorSSL(uint32_t error_code) {
  DCHECK_EQ(ERR_LIB_SSL, ERR_GET_LIB(error_code));

  DVLOG(1) << "OpenSSL SSL error, reason: " << ERR_GET_REASON(error_code)
           << ", name: " << ERR_error_string(error_code, NULL);
  switch (ERR_GET_REASON(error_code)) {
    case SSL_R_READ_TIMEOUT_EXPIRED:
      return ERR_TIMED_OUT;
    case SSL_R_UNKNOWN_CERTIFICATE_TYPE:
    case SSL_R_UNKNOWN_CIPHER_TYPE:
    case SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE:
    case SSL_R_UNKNOWN_PKEY_TYPE:
    case SSL_R_UNKNOWN_SSL_VERSION:
      return ERR_NOT_IMPLEMENTED;
    case SSL_R_UNSUPPORTED_SSL_VERSION:
    case SSL_R_NO_CIPHER_MATCH:
    case SSL_R_NO_SHARED_CIPHER:
    case SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY:
    case SSL_R_TLSV1_ALERT_PROTOCOL_VERSION:
    case SSL_R_UNSUPPORTED_PROTOCOL:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED:
    case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
    case SSL_R_TLSV1_ALERT_ACCESS_DENIED:
    case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
      return ERR_BAD_SSL_CLIENT_AUTH_CERT;
    case SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE:
      return ERR_SSL_DECOMPRESSION_FAILURE_ALERT;
    case SSL_R_SSLV3_ALERT_BAD_RECORD_MAC:
      return ERR_SSL_BAD_RECORD_MAC_ALERT;
    case SSL_R_TLSV1_ALERT_DECRYPT_ERROR:
      return ERR_SSL_DECRYPT_ERROR_ALERT;
    case SSL_R_TLSV1_UNRECOGNIZED_NAME:
      return ERR_SSL_UNRECOGNIZED_NAME_ALERT;
    case SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED:
      return ERR_SSL_UNSAFE_NEGOTIATION;
    case SSL_R_BAD_DH_P_LENGTH:
      return ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY;
    // SSL_R_UNKNOWN_PROTOCOL is reported if premature application data is
    // received (see http://crbug.com/42538), and also if all the protocol
    // versions supported by the server were disabled in this socket instance.
    // Mapped to ERR_SSL_PROTOCOL_ERROR for compatibility with other SSL sockets
    // in the former scenario.
    case SSL_R_UNKNOWN_PROTOCOL:
    case SSL_R_SSL_HANDSHAKE_FAILURE:
    case SSL_R_DECRYPTION_FAILED:
    case SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC:
    case SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG:
    case SSL_R_DIGEST_CHECK_FAILED:
    case SSL_R_ENCRYPTED_LENGTH_TOO_LONG:
    case SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST:
    case SSL_R_EXCESSIVE_MESSAGE_SIZE:
    case SSL_R_EXTRA_DATA_IN_MESSAGE:
    case SSL_R_GOT_A_FIN_BEFORE_A_CCS:
    case SSL_R_INVALID_COMMAND:
    case SSL_R_INVALID_STATUS_RESPONSE:
    case SSL_R_INVALID_TICKET_KEYS_LENGTH:
    // SSL_do_handshake reports this error when the server responds to a
    // ClientHello with a fatal close_notify alert.
    case SSL_AD_REASON_OFFSET + SSL_AD_CLOSE_NOTIFY:
    case SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE:
    // TODO(joth): SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE may be returned from the
    // server after receiving ClientHello if there's no common supported cipher.
    // Ideally we'd map that specific case to ERR_SSL_VERSION_OR_CIPHER_MISMATCH
    // to match the NSS implementation. See also http://goo.gl/oMtZW
    case SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE:
    case SSL_R_SSLV3_ALERT_NO_CERTIFICATE:
    case SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER:
    case SSL_R_TLSV1_ALERT_DECODE_ERROR:
    case SSL_R_TLSV1_ALERT_DECRYPTION_FAILED:
    case SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION:
    case SSL_R_TLSV1_ALERT_INTERNAL_ERROR:
    case SSL_R_TLSV1_ALERT_NO_RENEGOTIATION:
    case SSL_R_TLSV1_ALERT_RECORD_OVERFLOW:
    case SSL_R_TLSV1_ALERT_USER_CANCELLED:
      return ERR_SSL_PROTOCOL_ERROR;
    case SSL_R_CERTIFICATE_VERIFY_FAILED:
      // The only way that the certificate verify callback can fail is if
      // the leaf certificate changed during a renegotiation.
      return ERR_SSL_SERVER_CERT_CHANGED;
    case SSL_AD_REASON_OFFSET + SSL3_AD_INAPPROPRIATE_FALLBACK:
      return ERR_SSL_INAPPROPRIATE_FALLBACK;
    default:
      LOG(WARNING) << "Unmapped error reason: " << ERR_GET_REASON(error_code);
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

base::Value* NetLogOpenSSLErrorCallback(int net_error,
                                        int ssl_error,
                                        const OpenSSLErrorInfo& error_info,
                                        NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("net_error", net_error);
  dict->SetInteger("ssl_error", ssl_error);
  if (error_info.error_code != 0) {
    dict->SetInteger("error_lib", ERR_GET_LIB(error_info.error_code));
    dict->SetInteger("error_reason", ERR_GET_REASON(error_info.error_code));
  }
  if (error_info.file != NULL)
    dict->SetString("file", error_info.file);
  if (error_info.line != 0)
    dict->SetInteger("line", error_info.line);
  return dict;
}

}  // namespace

void OpenSSLPutNetError(const tracked_objects::Location& location, int err) {
  // Net error codes are negative. Encode them as positive numbers.
  err = -err;
  if (err < 0 || err > 0xfff) {
    // OpenSSL reserves 12 bits for the reason code.
    NOTREACHED();
    err = ERR_INVALID_ARGUMENT;
  }
  ERR_put_error(OpenSSLNetErrorLib(), 0, err,
                location.file_name(), location.line_number());
}

int MapOpenSSLError(int err, const crypto::OpenSSLErrStackTracer& tracer) {
  OpenSSLErrorInfo error_info;
  return MapOpenSSLErrorWithDetails(err, tracer, &error_info);
}

int MapOpenSSLErrorWithDetails(int err,
                               const crypto::OpenSSLErrStackTracer& tracer,
                               OpenSSLErrorInfo* out_error_info) {
  *out_error_info = OpenSSLErrorInfo();

  switch (err) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      return ERR_IO_PENDING;
    case SSL_ERROR_SYSCALL:
      LOG(ERROR) << "OpenSSL SYSCALL error, earliest error code in "
                    "error queue: " << ERR_peek_error() << ", errno: "
                 << errno;
      return ERR_FAILED;
    case SSL_ERROR_SSL:
      // Walk down the error stack to find an SSL or net error.
      uint32_t error_code;
      const char* file;
      int line;
      do {
        error_code = ERR_get_error_line(&file, &line);
        if (ERR_GET_LIB(error_code) == ERR_LIB_SSL) {
          out_error_info->error_code = error_code;
          out_error_info->file = file;
          out_error_info->line = line;
          return MapOpenSSLErrorSSL(error_code);
        } else if (ERR_GET_LIB(error_code) == OpenSSLNetErrorLib()) {
          out_error_info->error_code = error_code;
          out_error_info->file = file;
          out_error_info->line = line;
          // Net error codes are negative but encoded in OpenSSL as positive
          // numbers.
          return -ERR_GET_REASON(error_code);
        }
      } while (error_code != 0);
      return ERR_FAILED;
    default:
      // TODO(joth): Implement full mapping.
      LOG(WARNING) << "Unknown OpenSSL error " << err;
      return ERR_SSL_PROTOCOL_ERROR;
  }
}

NetLog::ParametersCallback CreateNetLogOpenSSLErrorCallback(
    int net_error,
    int ssl_error,
    const OpenSSLErrorInfo& error_info) {
  return base::Bind(&NetLogOpenSSLErrorCallback,
                    net_error, ssl_error, error_info);
}

}  // namespace net
