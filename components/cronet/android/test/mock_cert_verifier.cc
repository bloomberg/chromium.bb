// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_cert_verifier.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "jni/MockCertVerifier_jni.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/cert_test_util.h"

namespace cronet {

static jlong CreateMockCertVerifier(JNIEnv* env,
                                    const JavaParamRef<jclass>& jcaller,
                                    const JavaParamRef<jobjectArray>& jcerts) {
  std::vector<std::string> certs;
  base::android::AppendJavaStringArrayToStringVector(env, jcerts, &certs);
  net::MockCertVerifier* mock_cert_verifier = new net::MockCertVerifier();
  for (auto cert : certs) {
    net::CertVerifyResult verify_result;
    verify_result.verified_cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), cert);
    mock_cert_verifier->AddResultForCert(verify_result.verified_cert.get(),
                                         verify_result, net::OK);
  }

  return reinterpret_cast<jlong>(mock_cert_verifier);
}

bool RegisterMockCertVerifier(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
