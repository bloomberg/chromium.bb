// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/test/native_test_server.h"

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/cronet/android/test/cronet_test_util.h"
#include "jni/NativeTestServer_jni.h"
#include "net/base/host_port_pair.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace cronet {

namespace {

const char kEchoBodyPath[] = "/echo_body";
const char kEchoHeaderPath[] = "/echo_header";
const char kEchoAllHeadersPath[] = "/echo_all_headers";
const char kEchoMethodPath[] = "/echo_method";
const char kRedirectToEchoBodyPath[] = "/redirect_to_echo_body";
// Path that advertises the dictionary passed in query params if client
// supports Sdch encoding. E.g. /sdch/index?q=LeQxM80O will make the server
// responds with "Get-Dictionary: /sdch/dict/LeQxM80O".
const char kSdchPath[] = "/sdch/index";
// Path that returns encoded response if client has the right dictionary.
const char kSdchTestPath[] = "/sdch/test";
// Path where dictionaries are stored.
const char kSdchDictPath[] = "/sdch/dict/";

net::EmbeddedTestServer* g_test_server = nullptr;

scoped_ptr<net::test_server::RawHttpResponse> ConstructResponseBasedOnFile(
    const base::FilePath& file_path) {
  std::string file_contents;
  bool read_file = base::ReadFileToString(file_path, &file_contents);
  DCHECK(read_file);
  base::FilePath headers_path(
      file_path.AddExtension(FILE_PATH_LITERAL("mock-http-headers")));
  std::string headers_contents;
  bool read_headers = base::ReadFileToString(headers_path, &headers_contents);
  DCHECK(read_headers);
  scoped_ptr<net::test_server::RawHttpResponse> http_response(
      new net::test_server::RawHttpResponse(headers_contents, file_contents));
  return http_response;
}

scoped_ptr<net::test_server::HttpResponse> NativeTestServerRequestHandler(
    const net::test_server::HttpRequest& request) {
  DCHECK(g_test_server);
  scoped_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content_type("text/plain");

  if (request.relative_url == kEchoBodyPath) {
    if (request.has_content) {
      response->set_content(request.content);
    } else {
      response->set_content("Request has no body. :(");
    }
    return std::move(response);
  }

  if (base::StartsWith(request.relative_url, kEchoHeaderPath,
                       base::CompareCase::SENSITIVE)) {
    GURL url = g_test_server->GetURL(request.relative_url);
    auto it = request.headers.find(url.query());
    if (it != request.headers.end()) {
      response->set_content(it->second);
    } else {
      response->set_content("Header not found. :(");
    }
    return std::move(response);
  }

  if (request.relative_url == kEchoAllHeadersPath) {
    response->set_content(request.all_headers);
    return std::move(response);
  }

  if (request.relative_url == kEchoMethodPath) {
    response->set_content(request.method_string);
    return std::move(response);
  }

  if (request.relative_url == kRedirectToEchoBodyPath) {
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->AddCustomHeader("Location", kEchoBodyPath);
    return std::move(response);
  }

  // Unhandled requests result in the Embedded test server sending a 404.
  return scoped_ptr<net::test_server::BasicHttpResponse>();
}

scoped_ptr<net::test_server::HttpResponse> SdchRequestHandler(
    const net::test_server::HttpRequest& request) {
  DCHECK(g_test_server);
  base::FilePath dir_path;
  bool get_data_dir = base::android::GetDataDirectory(&dir_path);
  DCHECK(get_data_dir);
  dir_path = dir_path.Append(FILE_PATH_LITERAL("test"));

  if (base::StartsWith(request.relative_url, kSdchPath,
                       base::CompareCase::SENSITIVE)) {
    base::FilePath file_path = dir_path.Append("sdch/index");
    scoped_ptr<net::test_server::RawHttpResponse> response =
        ConstructResponseBasedOnFile(file_path);
    // Check for query params to see which dictionary to advertise.
    // For instance, ?q=dictionaryA will make the server advertise dictionaryA.
    GURL url = g_test_server->GetURL(request.relative_url);
    std::string dictionary;
    if (!net::GetValueForKeyInQuery(url, "q", &dictionary)) {
      CHECK(false) << "dictionary is not found in query params of "
                   << request.relative_url;
    }
    auto accept_encoding_header = request.headers.find("Accept-Encoding");
    if (accept_encoding_header != request.headers.end()) {
      if (accept_encoding_header->second.find("sdch") != std::string::npos)
        response->AddHeader(base::StringPrintf(
            "Get-Dictionary: %s%s", kSdchDictPath, dictionary.c_str()));
    }
    return std::move(response);
  }

  if (base::StartsWith(request.relative_url, kSdchTestPath,
                       base::CompareCase::SENSITIVE)) {
    auto avail_dictionary_header = request.headers.find("Avail-Dictionary");
    if (avail_dictionary_header != request.headers.end()) {
      base::FilePath file_path = dir_path.Append(
          "sdch/" + avail_dictionary_header->second + "_encoded");
      return ConstructResponseBasedOnFile(file_path);
    }
    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse());
    response->set_content_type("text/plain");
    response->set_content("Sdch is not used.\n");
    return std::move(response);
  }

  // Unhandled requests result in the Embedded test server sending a 404.
  return scoped_ptr<net::test_server::BasicHttpResponse>();
}

}  // namespace

jboolean StartNativeTestServer(JNIEnv* env,
                               const JavaParamRef<jclass>& jcaller,
                               const JavaParamRef<jstring>& jtest_files_root) {
  // Shouldn't happen.
  if (g_test_server)
    return false;
  g_test_server = new net::EmbeddedTestServer();
  g_test_server->RegisterRequestHandler(
      base::Bind(&NativeTestServerRequestHandler));
  g_test_server->RegisterRequestHandler(base::Bind(&SdchRequestHandler));
  base::FilePath test_files_root(
      base::android::ConvertJavaStringToUTF8(env, jtest_files_root));

  // Add a third handler for paths that NativeTestServerRequestHandler does not
  // handle.
  g_test_server->ServeFilesFromDirectory(test_files_root);
  return g_test_server->Start();
}

void ShutdownNativeTestServer(JNIEnv* env,
                              const JavaParamRef<jclass>& jcaller) {
  if (!g_test_server)
    return;
  delete g_test_server;
  g_test_server = NULL;
}

ScopedJavaLocalRef<jstring> GetEchoBodyURL(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoBodyPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec());
}

ScopedJavaLocalRef<jstring> GetEchoHeaderURL(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jstring>& jheader) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoHeaderPath);
  GURL::Replacements replacements;
  std::string header = base::android::ConvertJavaStringToUTF8(env, jheader);
  replacements.SetQueryStr(header.c_str());
  url = url.ReplaceComponents(replacements);
  return base::android::ConvertUTF8ToJavaString(env, url.spec());
}

ScopedJavaLocalRef<jstring> GetEchoAllHeadersURL(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoAllHeadersPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec());
}

ScopedJavaLocalRef<jstring> GetEchoMethodURL(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kEchoMethodPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec());
}

ScopedJavaLocalRef<jstring> GetRedirectToEchoBody(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  DCHECK(g_test_server);
  GURL url = g_test_server->GetURL(kRedirectToEchoBodyPath);
  return base::android::ConvertUTF8ToJavaString(env, url.spec());
}

ScopedJavaLocalRef<jstring> GetFileURL(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jstring>& jfile_path) {
  DCHECK(g_test_server);
  std::string file = base::android::ConvertJavaStringToUTF8(env, jfile_path);
  GURL url = g_test_server->GetURL(file);
  return base::android::ConvertUTF8ToJavaString(env, url.spec());
}

ScopedJavaLocalRef<jstring> GetSdchURL(JNIEnv* env,
                                       const JavaParamRef<jclass>& jcaller) {
  DCHECK(g_test_server);
  std::string url(base::StringPrintf("http://%s:%d", kFakeSdchDomain,
                                     g_test_server->port()));
  return base::android::ConvertUTF8ToJavaString(env, url);
}

ScopedJavaLocalRef<jstring> GetHostPort(JNIEnv* env,
                                        const JavaParamRef<jclass>& jcaller) {
  DCHECK(g_test_server);
  std::string host_port =
      net::HostPortPair::FromURL(g_test_server->base_url()).ToString();
  return base::android::ConvertUTF8ToJavaString(env, host_port);
}

jboolean IsDataReductionProxySupported(JNIEnv* env,
                                       const JavaParamRef<jclass>& jcaller) {
#if defined(DATA_REDUCTION_PROXY_SUPPORT)
  return JNI_TRUE;
#else
  return JNI_FALSE;
#endif
}

bool RegisterNativeTestServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
