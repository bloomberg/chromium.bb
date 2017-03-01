// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/page_info/certificate_chain_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "jni/CertificateChainHelper_jni.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

static ScopedJavaLocalRef<jobjectArray> GetCertificateChain(
    JNIEnv* env,
    const JavaParamRef<jclass>&,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (!web_contents)
    return ScopedJavaLocalRef<jobjectArray>();

  scoped_refptr<net::X509Certificate> cert =
      web_contents->GetController().GetVisibleEntry()->GetSSL().certificate;
  if (!cert)
    return ScopedJavaLocalRef<jobjectArray>();

  std::vector<std::string> cert_chain;
  net::X509Certificate::OSCertHandles cert_handles =
      cert->GetIntermediateCertificates();
  // Make sure the peer's own cert is the first in the chain, if it's not
  // already there.
  if (cert_handles.empty() || cert_handles[0] != cert->os_cert_handle())
    cert_handles.insert(cert_handles.begin(), cert->os_cert_handle());

  cert_chain.reserve(cert_handles.size());
  for (auto* handle : cert_handles) {
    std::string cert_bytes;
    net::X509Certificate::GetDEREncoded(handle, &cert_bytes);
    cert_chain.push_back(cert_bytes);
  }

  return base::android::ToJavaArrayOfByteArray(env, cert_chain);
}

// static
bool RegisterCertificateChainHelper(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
