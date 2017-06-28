// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/test/experimental_options_test.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "components/cronet/android/test/cronet_test_util.h"
#include "jni/ExperimentalOptionsTest_jni.h"
#include "net/base/address_family.h"
#include "net/base/net_errors.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/url_request/url_request_context.h"

using base::android::JavaParamRef;

namespace cronet {

namespace {
void WriteToHostCacheOnNetworkThread(jlong jcontext_adapter,
                                     const std::string& address_string) {
  net::URLRequestContext* context =
      TestUtil::GetURLRequestContext(jcontext_adapter);
  net::HostCache* cache = context->host_resolver()->GetHostCache();
  net::HostCache::Key key("host-cache-test-host",
                          net::ADDRESS_FAMILY_UNSPECIFIED, 0);
  net::IPAddress address;
  CHECK(address.AssignFromIPLiteral(address_string));
  net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(address, 0);
  net::HostCache::Entry entry(net::OK, address_list);
  cache->Set(key, entry, base::TimeTicks::Now(),
             base::TimeDelta::FromSeconds(1));
}
}  // namespace

static void WriteToHostCache(JNIEnv* env,
                             const JavaParamRef<jclass>& jcaller,
                             jlong jcontext_adapter,
                             const JavaParamRef<jstring>& jaddress) {
  TestUtil::RunAfterContextInit(
      jcontext_adapter,
      base::Bind(&WriteToHostCacheOnNetworkThread, jcontext_adapter,
                 base::android::ConvertJavaStringToUTF8(env, jaddress)));
}

bool RegisterExperimentalOptionsTest(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
