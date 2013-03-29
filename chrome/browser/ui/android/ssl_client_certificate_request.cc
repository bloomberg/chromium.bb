// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/ssl_client_certificate_request.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "content/public/browser/browser_thread.h"
#include "jni/SSLClientCertificateRequest_jni.h"
#include "net/android/keystore_openssl.h"
#include "net/base/host_port_pair.h"
#include "net/base/openssl_client_key_store.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_client_cert_type.h"

namespace chrome {

namespace {

typedef net::OpenSSLClientKeyStore::ScopedEVP_PKEY ScopedEVP_PKEY;

// Must be called on the I/O thread to record a client certificate
// and its private key in the OpenSSLClientKeyStore.
void RecordClientCertificateKey(
    const scoped_refptr<net::X509Certificate>& client_cert,
    ScopedEVP_PKEY private_key) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  net::OpenSSLClientKeyStore::GetInstance()->RecordClientCertPrivateKey(
      client_cert.get(), private_key.get());
}

void StartClientCertificateRequest(
    const net::SSLCertRequestInfo* cert_request_info,
    const chrome::SelectCertificateCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Ensure that callback(NULL) is posted as a task on the UI thread
  // in case of an error.
  base::Closure post_task_closure = base::Bind(
      base::IgnoreResult(&content::BrowserThread::PostTask),
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, scoped_refptr<net::X509Certificate>()));

  base::ScopedClosureRunner guard(post_task_closure);

  // Build the |key_types| JNI parameter, as a String[]
  std::vector<std::string> key_types;
  for (size_t n = 0; n < cert_request_info->cert_key_types.size(); ++n) {
    switch (cert_request_info->cert_key_types[n]) {
      case net::CLIENT_CERT_RSA_SIGN:
        key_types.push_back("RSA");
        break;
      case net::CLIENT_CERT_DSS_SIGN:
        key_types.push_back("DSA");
        break;
      case net::CLIENT_CERT_ECDSA_SIGN:
        key_types.push_back("ECDSA");
        break;
      default:
        // Ignore unknown types.
        break;
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> key_types_ref =
      base::android::ToJavaArrayOfStrings(env, key_types);
  if (key_types_ref.is_null()) {
    LOG(ERROR) << "Could not create key types array (String[])";
    return;
  }

  // Build the |encoded_principals| JNI parameter, as a byte[][]
  ScopedJavaLocalRef<jobjectArray> principals_ref =
      base::android::ToJavaArrayOfByteArray(
          env, cert_request_info->cert_authorities);
  if (principals_ref.is_null()) {
    LOG(ERROR) << "Could not create principals array (byte[][])";
    return;
  }

  // Build the |host_name| and |port| JNI parameters, as a String and
  // a jint.
  net::HostPortPair host_and_port =
      net::HostPortPair::FromString(cert_request_info->host_and_port);

  ScopedJavaLocalRef<jstring> host_name_ref =
      base::android::ConvertUTF8ToJavaString(env, host_and_port.host());
  if (host_name_ref.is_null()) {
    LOG(ERROR) << "Could not extract host name from: '"
               << cert_request_info->host_and_port << "'";
    return;
  }

  // Create a copy of the callback on the heap so that its address
  // and ownership can be passed through and returned from Java via JNI.
  scoped_ptr<chrome::SelectCertificateCallback> request(
      new chrome::SelectCertificateCallback(callback));

  jint request_id = reinterpret_cast<jint>(request.get());

  if (!chrome::android::
      Java_SSLClientCertificateRequest_selectClientCertificate(
          env, request_id, key_types_ref.obj(), principals_ref.obj(),
          host_name_ref.obj(), host_and_port.port())) {
    return;
  }

  guard.Release();

  // Ownership was transferred to Java.
  chrome::SelectCertificateCallback* ALLOW_UNUSED dummy =
      request.release();
}

}  // namespace

namespace android {

// Called from JNI on request completion/result.
// |env| is the current thread's JNIEnv.
// |clazz| is the SSLClientCertificateRequest JNI class reference.
// |request_id| is the id passed to
// Java_SSLClientCertificateRequest_selectClientCertificate() in Start().
// |encoded_chain_ref| is a JNI reference to a Java array of byte arrays,
// each item holding a DER-encoded X.509 certificate.
// |private_key_ref| is the platform PrivateKey object JNI reference for
// the client certificate.
// Note: both |encoded_chain_ref| and |private_key_ref| will be NULL if
// the user didn't select a certificate.
static void OnSystemRequestCompletion(
    JNIEnv* env,
    jclass clazz,
    jint request_id,
    jobjectArray encoded_chain_ref,
    jobject private_key_ref) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Take back ownership of the request object.
  scoped_ptr<chrome::SelectCertificateCallback> callback(
      reinterpret_cast<chrome::SelectCertificateCallback*>(request_id));

  // Ensure that callback(NULL) is called in case of an error.
  base::Closure null_closure =
      base::Bind(*callback, scoped_refptr<net::X509Certificate>());

  base::ScopedClosureRunner guard(null_closure);

  if (encoded_chain_ref == NULL || private_key_ref == NULL) {
    LOG(ERROR) << "Client certificate request cancelled";
    return;
  }

  // Convert the encoded chain to a vector of strings.
  std::vector<std::string> encoded_chain_strings;
  if (encoded_chain_ref) {
    base::android::JavaArrayOfByteArrayToStringVector(
        env, encoded_chain_ref, &encoded_chain_strings);
  }

  std::vector<base::StringPiece> encoded_chain;
  for (size_t n = 0; n < encoded_chain_strings.size(); ++n)
    encoded_chain.push_back(encoded_chain_strings[n]);

  // Create the X509Certificate object from the encoded chain.
  scoped_refptr<net::X509Certificate> client_cert(
      net::X509Certificate::CreateFromDERCertChain(encoded_chain));
  if (!client_cert.get()) {
    LOG(ERROR) << "Could not decode client certificate chain";
    return;
  }

  // Create an EVP_PKEY wrapper for the private key JNI reference.
  ScopedEVP_PKEY private_key(
      net::android::GetOpenSSLPrivateKeyWrapper(private_key_ref));
  if (!private_key.get()) {
    LOG(ERROR) << "Could not create OpenSSL wrapper for private key";
    return;
  }

  guard.Release();

  // RecordClientCertificateKey() must be called on the I/O thread,
  // before the callback is called with the selected certificate on
  // the UI thread.
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&RecordClientCertificateKey,
                 client_cert,
                 base::Passed(&private_key)),
      base::Bind(*callback, client_cert));
}

bool RegisterSSLClientCertificateRequestAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android

void ShowSSLClientCertificateSelector(
    content::WebContents* contents,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const chrome::SelectCertificateCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  StartClientCertificateRequest(cert_request_info, callback);
}

}  // namespace chrome
