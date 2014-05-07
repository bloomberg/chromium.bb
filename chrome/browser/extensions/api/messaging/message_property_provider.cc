// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/message_property_provider.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/api/runtime.h"
#include "net/base/completion_callback.h"
#include "net/cert/asn1_util.h"
#include "net/cert/jwk_serializer.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace extensions {

MessagePropertyProvider::MessagePropertyProvider() {}

void MessagePropertyProvider::GetDomainBoundCert(Profile* profile,
    const GURL& source_url, const DomainBoundCertCallback& reply) {
  if (!source_url.is_valid()) {
    // This isn't a real URL, so there's no sense in looking for a channel ID
    // for it. Dispatch with an empty tls channel ID.
    reply.Run(std::string());
    return;
  }
  scoped_refptr<net::URLRequestContextGetter> request_context_getter(
      profile->GetRequestContext());
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&MessagePropertyProvider::GetDomainBoundCertOnIOThread,
                 base::MessageLoopProxy::current(),
                 request_context_getter,
                 source_url.host(),
                 reply));
}

// Helper struct to bind the memory addresses that will be written to by
// ServerBoundCertService::GetDomainBoundCert to the callback provided to
// MessagePropertyProvider::GetDomainBoundCert.
struct MessagePropertyProvider::GetDomainBoundCertOutput {
  std::string domain_bound_private_key;
  std::string domain_bound_cert;
  net::ServerBoundCertService::RequestHandle request_handle;
};

// static
void MessagePropertyProvider::GetDomainBoundCertOnIOThread(
    scoped_refptr<base::TaskRunner> original_task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    const std::string& host,
    const DomainBoundCertCallback& reply) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::ServerBoundCertService* server_bound_cert_service =
      request_context_getter->GetURLRequestContext()->
          server_bound_cert_service();
  GetDomainBoundCertOutput* output = new GetDomainBoundCertOutput();
  net::CompletionCallback net_completion_callback =
      base::Bind(&MessagePropertyProvider::GotDomainBoundCert,
                 original_task_runner,
                 base::Owned(output),
                 reply);
  int status = server_bound_cert_service->GetDomainBoundCert(
      host,
      &output->domain_bound_private_key,
      &output->domain_bound_cert,
      net_completion_callback,
      &output->request_handle);
  if (status == net::ERR_IO_PENDING)
    return;
  GotDomainBoundCert(original_task_runner, output, reply, status);
}

// static
void MessagePropertyProvider::GotDomainBoundCert(
    scoped_refptr<base::TaskRunner> original_task_runner,
    struct GetDomainBoundCertOutput* output,
    const DomainBoundCertCallback& reply,
    int status) {
  base::Closure no_tls_channel_id_closure = base::Bind(reply, "");
  if (status != net::OK) {
    original_task_runner->PostTask(FROM_HERE, no_tls_channel_id_closure);
    return;
  }
  base::StringPiece spki;
  if (!net::asn1::ExtractSPKIFromDERCert(output->domain_bound_cert, &spki)) {
    original_task_runner->PostTask(FROM_HERE, no_tls_channel_id_closure);
    return;
  }
  base::DictionaryValue jwk_value;
  if (!net::JwkSerializer::ConvertSpkiFromDerToJwk(spki, &jwk_value)) {
    original_task_runner->PostTask(FROM_HERE, no_tls_channel_id_closure);
    return;
  }
  std::string jwk_str;
  base::JSONWriter::Write(&jwk_value, &jwk_str);
  original_task_runner->PostTask(FROM_HERE, base::Bind(reply, jwk_str));
}

}  // namespace extensions
