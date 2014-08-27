// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/grit/generated_resources.h"
#include "jni/CertificateViewer_jni.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  // For Android, showing the certificate is always handled in Java.
  NOTREACHED();
}

static jstring GetCertIssuedToText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_SUBJECT_GROUP)).Release();
}

static jstring GetCertInfoCommonNameText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_COMMON_NAME_LABEL)).Release();
}

static jstring GetCertInfoOrganizationText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(
          IDS_CERT_INFO_ORGANIZATION_LABEL)).Release();
}

static jstring GetCertInfoSerialNumberText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(
          IDS_CERT_INFO_SERIAL_NUMBER_LABEL)).Release();
}

static jstring GetCertInfoOrganizationUnitText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(
          IDS_CERT_INFO_ORGANIZATIONAL_UNIT_LABEL)).Release();
}

static jstring GetCertIssuedByText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_ISSUER_GROUP)).Release();
}

static jstring GetCertValidityText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_VALIDITY_GROUP)).Release();
}

static jstring GetCertIssuedOnText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_ISSUED_ON_LABEL)).Release();
}

static jstring GetCertExpiresOnText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_CERT_INFO_EXPIRES_ON_LABEL)).Release();
}

static jstring GetCertFingerprintsText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(
          IDS_CERT_INFO_FINGERPRINTS_GROUP)).Release();
}

static jstring GetCertSHA256FingerprintText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(
          IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL)).Release();
}

static jstring GetCertSHA1FingerprintText(JNIEnv* env, jclass) {
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(
          IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL)).Release();
}

bool RegisterCertificateViewer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
