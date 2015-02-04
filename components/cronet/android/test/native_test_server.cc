// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_test_server.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "jni/NativeTestServer_jni.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace cronet {

namespace {

const char echo_body_path[] = "/echo_body";
const char echo_header_path[] = "/echo_header";
const char echo_all_headers_path[] = "/echo_all_headers";
const char echo_method_path[] = "/echo_method";
const char redirect_to_echo_body_path[] = "/redirect_to_echo_body";

net::test_server::EmbeddedTestServer* g_test_server = nullptr;

scoped_ptr<net::test_server::HttpResponse> UploadServerRequestHandler(
    const net::test_server::HttpRequest& request) {
  DCHECK(g_test_server);
  scoped_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content_type("text/plain");

  if (request.relative_url == echo_body_path) {
    if (request.has_content) {
      response->set_content(request.content);
    } else {
      response->set_content("Request has no body. :(");
    }
    return response.Pass();
  }

  if (StartsWithASCII(request.relative_url, echo_header_path, true)) {
    GURL url = g_test_server->GetURL(request.relative_url);
    auto it = request.headers.find(url.query());
    if (it != request.headers.end()) {
      response->set_content(it->second);
    } else {
      response->set_content("Header not found. :(");
    }
    return response.Pass();
  }

  if (request.relative_url == echo_all_headers_path) {
    response->set_content(request.all_headers);
    return response.Pass();
  }

  if (request.relative_url == echo_method_path) {
    response->set_content(request.method_string);
    return response.Pass();
  }

  if (request.relative_url == redirect_to_echo_body_path) {
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->AddCustomHeader("Location", echo_body_path);
    return response.Pass();
  }

  // Unhandled requests result in the Embedded test server sending a 404.
  return scoped_ptr<net::test_server::BasicHttpResponse>();
}

}  // namespace

jboolean StartNativeTestServer(JNIEnv* env,
                               jclass jcaller,
                               jstring jtest_files_root) {
  // Shouldn't happen.
  if (g_test_server)
    return false;
  g_test_server = new net::test_server::EmbeddedTestServer();
  g_test_server->RegisterRequestHandler(
      base::Bind(&UploadServerRequestHandler));
  // Add a second handler for paths that UploadServerRequestHandler does not
  // handle.
  base::FilePath test_files_root(
      base::android::ConvertJavaStringToUTF8(env, jtest_files_root));
  g_test_server->ServeFilesFromDirectory(test_files_root);
  return g_test_server->InitializeAndWaitUntilReady();
}

void ShutdownNativeTestServer(JNIEnv* env, jclass jcaller) {
  if (!g_test_server)
    return;
  delete g_test_server;
  g_test_server = NULL;
}

jstring GetEchoBodyURL(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(echo_body_path);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetEchoHeaderURL(JNIEnv* env, jclass jcaller, jstring jheader) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(echo_header_path);
  GURL::Replacements replacements;
  std::string header = base::android::ConvertJavaStringToUTF8(env, jheader);
  replacements.SetQueryStr(header.c_str());
  url = url.ReplaceComponents(replacements);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetEchoAllHeadersURL(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(echo_all_headers_path);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetEchoMethodURL(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(echo_method_path);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetRedirectToEchoBody(JNIEnv* env, jclass jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(redirect_to_echo_body_path);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

jstring GetFileURL(JNIEnv* env, jclass jcaller, jstring jfile_path) {
  DCHECK(g_test_server);
  std::string file = base::android::ConvertJavaStringToUTF8(env, jfile_path);
  GURL url = g_test_server->GetURL(file);
  return base::android::ConvertUTF8ToJavaString(env, url.spec()).Release();
}

bool RegisterNativeTestServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
