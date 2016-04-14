// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/net_jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "net/android/gurl_utils.h"
#include "net/android/http_auth_negotiate_android.h"
#include "net/android/keystore.h"
#include "net/android/network_change_notifier_android.h"
#include "net/android/network_library.h"
#include "net/android/traffic_stats.h"
#include "net/cert/x509_util_android.h"
#include "net/proxy/proxy_config_service_android.h"
#include "url/url_features.h"

#if BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
#include "net/base/net_string_util_icu_alternatives_android.h"  // nogncheck
#endif

namespace net {
namespace android {

static base::android::RegistrationMethod kNetRegisteredMethods[] = {
    {"AndroidCertVerifyResult", RegisterCertVerifyResult},
    {"AndroidKeyStore", RegisterKeyStore},
    {"AndroidNetworkLibrary", RegisterNetworkLibrary},
    {"AndroidTrafficStats", traffic_stats::Register},
    {"GURLUtils", RegisterGURLUtils},
    {"HttpAuthNegotiateAndroid", HttpAuthNegotiateAndroid::Register},
    {"NetworkChangeNotifierAndroid", NetworkChangeNotifierAndroid::Register},
    {"ProxyConfigService", ProxyConfigServiceAndroid::Register},
    {"X509Util", RegisterX509Util},
#if BUILDFLAG(USE_PLATFORM_ICU_ALTERNATIVES)
    {"NetStringUtils", RegisterNetStringUtils}
#endif
};

bool RegisterJni(JNIEnv* env) {
  return base::android::RegisterNativeMethods(
      env, kNetRegisteredMethods, arraysize(kNetRegisteredMethods));
}

}  // namespace android
}  // namespace net
