// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// URL request job for reading from resources and assets.

#include "chrome/browser/android/android_protocol_adapter.h"

#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/string_util.h"
#include "chrome/browser/android/android_stream_reader_url_request_job.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "jni/AndroidProtocolAdapter_jni.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_job_manager.h"

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Override resource context for reading resource and asset files. Used for
// testing.
JavaObjectWeakGlobalRef* g_resource_context = NULL;

void ResetResourceContext(JavaObjectWeakGlobalRef* ref) {
  if (g_resource_context)
    delete g_resource_context;

  g_resource_context = ref;
}

class AndroidStreamReaderURLRequestJobDelegateImpl
    : public AndroidStreamReaderURLRequestJob::Delegate {
 public:
  AndroidStreamReaderURLRequestJobDelegateImpl();

  virtual ScopedJavaLocalRef<jobject> OpenInputStream(
      JNIEnv* env,
      net::URLRequest* request) OVERRIDE;

  virtual bool GetMimeType(JNIEnv* env,
                           net::URLRequest* request,
                           jobject stream,
                           std::string* mime_type) OVERRIDE;

  virtual bool GetCharset(JNIEnv* env,
                          net::URLRequest* request,
                          jobject stream,
                          std::string* charset) OVERRIDE;

  virtual ~AndroidStreamReaderURLRequestJobDelegateImpl();
};


} // namespace

static bool InitJNIBindings(JNIEnv* env) {
  return RegisterNativesImpl(env) &&
      AndroidStreamReaderURLRequestJob::InitJNIBindings(env);
}

// static
net::URLRequestJob* AndroidProtocolAdapter::Factory(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  DCHECK(scheme == chrome::kFileScheme ||
         scheme == chrome::kContentScheme);
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  // If this is a file:// URL we cannot handle, fall back to the default
  // handler.
  const std::string& url = request->url().spec();
  std::string assetPrefix = std::string(chrome::kFileScheme) + "://" +
      chrome::kAndroidAssetPath;
  std::string resourcePrefix = std::string(chrome::kFileScheme) + "://" +
      chrome::kAndroidResourcePath;

  if (scheme == chrome::kFileScheme &&
      !StartsWithASCII(url, assetPrefix, /*case_sensitive=*/ true) &&
      !StartsWithASCII(url, resourcePrefix,  /*case_sensitive=*/ true)) {
    return net::URLRequestFileJob::Factory(request, network_delegate, scheme);
  }

  return new AndroidStreamReaderURLRequestJob(
      request,
      network_delegate,
      scoped_ptr<AndroidStreamReaderURLRequestJob::Delegate>(
          new AndroidStreamReaderURLRequestJobDelegateImpl()));
}

// static
bool AndroidProtocolAdapter::RegisterProtocols(JNIEnv* env) {
  DCHECK(env);

  if (!InitJNIBindings(env))
    return false;

  // Register content:// and file://. Note that even though a scheme is
  // registered here, it cannot be used by child processes until access to it is
  // granted via ChildProcessSecurityPolicy::GrantScheme(). This is done in
  // RenderViewHost.
  net::URLRequestJobManager* job_manager =
      net::URLRequestJobManager::GetInstance();
  job_manager->RegisterProtocolFactory(chrome::kContentScheme,
      &AndroidProtocolAdapter::Factory);
  job_manager->RegisterProtocolFactory(
      chrome::kFileScheme, &AndroidProtocolAdapter::Factory);

  return true;
}

// Set a context object to be used for resolving resource queries. This can
// be used to override the default application context and redirect all
// resource queries to a specific context object, e.g., for the purposes of
// testing.
//
// |context| should be a android.content.Context instance or NULL to enable
// the use of the standard application context.
static void SetResourceContextForTesting(JNIEnv* env, jclass /*clazz*/,
                                         jobject context) {
  if (context) {
    ResetResourceContext(new JavaObjectWeakGlobalRef(env, context));
  } else {
    ResetResourceContext(NULL);
  }
}

static jstring GetAndroidAssetPath(JNIEnv* env, jclass /*clazz*/) {
  // OK to release, JNI binding.
  return ConvertUTF8ToJavaString(env, chrome::kAndroidAssetPath).Release();
}

static jstring GetAndroidResourcePath(JNIEnv* env, jclass /*clazz*/) {
  // OK to release, JNI binding.
  return ConvertUTF8ToJavaString(env, chrome::kAndroidResourcePath).Release();
}

static ScopedJavaLocalRef<jobject> GetResourceContext(JNIEnv* env) {
  if (g_resource_context)
    return g_resource_context->get(env);
  ScopedJavaLocalRef<jobject> context;
  // We have to reset as GetApplicationContext() returns a jobject with a
  // global ref. The constructor that takes a jobject would expect a local ref
  // and would assert.
  context.Reset(env, base::android::GetApplicationContext());
  return context;
}

AndroidStreamReaderURLRequestJobDelegateImpl::
AndroidStreamReaderURLRequestJobDelegateImpl() {
}

AndroidStreamReaderURLRequestJobDelegateImpl::
~AndroidStreamReaderURLRequestJobDelegateImpl() {
}

ScopedJavaLocalRef<jobject>
AndroidStreamReaderURLRequestJobDelegateImpl::OpenInputStream(
    JNIEnv* env, net::URLRequest* request) {
  DCHECK(request);
  DCHECK(env);

  // Open the input stream.
  ScopedJavaLocalRef<jstring> url =
      ConvertUTF8ToJavaString(env, request->url().spec());
  ScopedJavaLocalRef<jobject> stream = Java_AndroidProtocolAdapter_open(
      env,
      GetResourceContext(env).obj(),
      url.obj());

  // Check and clear pending exceptions.
  if (ClearException(env) || stream.is_null()) {
    DLOG(ERROR) << "Unable to open input stream for Android URL";
    return ScopedJavaLocalRef<jobject>(env, NULL);
  }
  return stream;
}

bool AndroidStreamReaderURLRequestJobDelegateImpl::GetMimeType(
    JNIEnv* env,
    net::URLRequest* request,
    jobject stream,
    std::string* mime_type) {
  DCHECK(env);
  DCHECK(request);
  DCHECK(mime_type);

  if (!stream)
    return false;

  // Query the mime type from the Java side. It is possible for the query to
  // fail, as the mime type cannot be determined for all supported schemes.
  ScopedJavaLocalRef<jstring> url =
      ConvertUTF8ToJavaString(env, request->url().spec());
  ScopedJavaLocalRef<jstring> returned_type =
      Java_AndroidProtocolAdapter_getMimeType(env,
                                              GetResourceContext(env).obj(),
                                              stream, url.obj());
  if (ClearException(env) || returned_type.is_null())
    return false;

  *mime_type = base::android::ConvertJavaStringToUTF8(returned_type);
  return true;
}

bool AndroidStreamReaderURLRequestJobDelegateImpl::GetCharset(
    JNIEnv* env,
    net::URLRequest* request,
    jobject stream,
    std::string* charset) {
  // TODO: We should probably be getting this from the managed side.
  return false;
}
