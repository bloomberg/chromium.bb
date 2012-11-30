// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_protocol_handler.h"

#include "android_webview/browser/net/android_stream_reader_url_request_job.h"
#include "android_webview/common/url_constants.h"
#include "android_webview/native/input_stream_impl.h"
#include "base/android/jni_android.h"
#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/string_util.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "jni/AndroidProtocolHandler_jni.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_factory.h"

using android_webview::InputStream;
using android_webview::InputStreamImpl;
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

  virtual scoped_ptr<InputStream> OpenInputStream(
      JNIEnv* env,
      net::URLRequest* request) OVERRIDE;

  virtual bool GetMimeType(JNIEnv* env,
                           net::URLRequest* request,
                           InputStream* stream,
                           std::string* mime_type) OVERRIDE;

  virtual bool GetCharset(JNIEnv* env,
                          net::URLRequest* request,
                          InputStream* stream,
                          std::string* charset) OVERRIDE;

  virtual ~AndroidStreamReaderURLRequestJobDelegateImpl();
};

class AssetFileProtocolInterceptor :
      public net::URLRequestJobFactory::Interceptor {
 public:
  AssetFileProtocolInterceptor();
  virtual ~AssetFileProtocolInterceptor() OVERRIDE;
  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      const GURL& location,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  // file:///android_asset/
  const std::string asset_prefix_;
  // file:///android_res/
  const std::string resource_prefix_;
};

// Protocol handler for content:// scheme requests.
class ContentSchemeProtocolHandler :
    public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ContentSchemeProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
  DCHECK(request->url().SchemeIs(android_webview::kContentScheme));
  return new AndroidStreamReaderURLRequestJob(
      request,
      network_delegate,
      scoped_ptr<AndroidStreamReaderURLRequestJob::Delegate>(
          new AndroidStreamReaderURLRequestJobDelegateImpl()));
  }
};

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

scoped_ptr<InputStream>
AndroidStreamReaderURLRequestJobDelegateImpl::OpenInputStream(
    JNIEnv* env, net::URLRequest* request) {
  DCHECK(request);
  DCHECK(env);

  // Open the input stream.
  ScopedJavaLocalRef<jstring> url =
      ConvertUTF8ToJavaString(env, request->url().spec());
  ScopedJavaLocalRef<jobject> stream =
      android_webview::Java_AndroidProtocolHandler_open(
          env,
          GetResourceContext(env).obj(),
          url.obj());

  // Check and clear pending exceptions.
  if (ClearException(env) || stream.is_null()) {
    DLOG(ERROR) << "Unable to open input stream for Android URL";
    return scoped_ptr<InputStream>();
  }
  return make_scoped_ptr<InputStream>(new InputStreamImpl(stream));
}

bool AndroidStreamReaderURLRequestJobDelegateImpl::GetMimeType(
    JNIEnv* env,
    net::URLRequest* request,
    android_webview::InputStream* stream,
    std::string* mime_type) {
  DCHECK(env);
  DCHECK(request);
  DCHECK(mime_type);

  // Query the mime type from the Java side. It is possible for the query to
  // fail, as the mime type cannot be determined for all supported schemes.
  ScopedJavaLocalRef<jstring> url =
      ConvertUTF8ToJavaString(env, request->url().spec());
  const InputStreamImpl* stream_impl =
      InputStreamImpl::FromInputStream(stream);
  ScopedJavaLocalRef<jstring> returned_type =
      android_webview::Java_AndroidProtocolHandler_getMimeType(
          env,
          GetResourceContext(env).obj(),
          stream_impl->jobj(), url.obj());
  if (ClearException(env) || returned_type.is_null())
    return false;

  *mime_type = base::android::ConvertJavaStringToUTF8(returned_type);
  return true;
}

bool AndroidStreamReaderURLRequestJobDelegateImpl::GetCharset(
    JNIEnv* env,
    net::URLRequest* request,
    android_webview::InputStream* stream,
    std::string* charset) {
  // TODO: We should probably be getting this from the managed side.
  return false;
}

AssetFileProtocolInterceptor::AssetFileProtocolInterceptor()
    : asset_prefix_(std::string(chrome::kFileScheme) +
                    std::string(content::kStandardSchemeSeparator) +
                    android_webview::kAndroidAssetPath),
      resource_prefix_(std::string(chrome::kFileScheme) +
                       std::string(content::kStandardSchemeSeparator) +
                       android_webview::kAndroidResourcePath) {
}

AssetFileProtocolInterceptor::~AssetFileProtocolInterceptor() {
}

net::URLRequestJob* AssetFileProtocolInterceptor::MaybeIntercept(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  if (!request->url().SchemeIsFile()) return NULL;

  const std::string& url = request->url().spec();
  if (!StartsWithASCII(url, asset_prefix_, /*case_sensitive=*/ true) &&
      !StartsWithASCII(url, resource_prefix_,  /*case_sensitive=*/ true)) {
    return NULL;
  }

  return new AndroidStreamReaderURLRequestJob(
      request,
      network_delegate,
      scoped_ptr<AndroidStreamReaderURLRequestJob::Delegate>(
          new AndroidStreamReaderURLRequestJobDelegateImpl()));
}

net::URLRequestJob* AssetFileProtocolInterceptor::MaybeInterceptRedirect(
    const GURL& location,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return NULL;
}

net::URLRequestJob* AssetFileProtocolInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return NULL;
}

}  // namespace

namespace android_webview {

bool RegisterAndroidProtocolHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void RegisterAndroidProtocolsOnIOThread(
    net::URLRequestContext* context,
    net::URLRequestJobFactory* job_factory) {
  // Register content://. Note that even though a scheme is
  // registered here, it cannot be used by child processes until access to it is
  // granted via ChildProcessSecurityPolicy::GrantScheme(). This is done in
  // AwContentBrowserClient.
  // The job factory takes ownership of the handler.
  job_factory->SetProtocolHandler(android_webview::kContentScheme,
                                  new ContentSchemeProtocolHandler());
  // The job factory takes ownership of the interceptor.
  job_factory->AddInterceptor(new AssetFileProtocolInterceptor());
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
  return ConvertUTF8ToJavaString(
      env, android_webview::kAndroidAssetPath).Release();
}

static jstring GetAndroidResourcePath(JNIEnv* env, jclass /*clazz*/) {
  // OK to release, JNI binding.
  return ConvertUTF8ToJavaString(
      env, android_webview::kAndroidResourcePath).Release();
}

}  // namespace android_webview
