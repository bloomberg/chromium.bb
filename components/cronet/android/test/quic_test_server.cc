// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic_test_server.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread.h"
#include "jni/QuicTestServer_jni.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_simple_server.h"

namespace cronet {

namespace {

static const char kServerHost[] = "127.0.0.1";
static const int kServerPort = 6121;

base::Thread* g_quic_server_thread = nullptr;
net::tools::QuicSimpleServer* g_quic_server = nullptr;

void ServeFilesFromDirectory(
    const base::FilePath& directory) {
  DCHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  DCHECK(!g_quic_server);
  base::FilePath file_dir = directory.Append("quic_data");
  CHECK(base::PathExists(file_dir)) << "Quic data does not exist";
  net::tools::QuicInMemoryCache::GetInstance()->InitializeFromDirectory(
      file_dir.value());
  net::IPAddressNumber ip;
  net::ParseIPLiteralToNumber(kServerHost, &ip);
  net::QuicConfig config;
  g_quic_server =
      new net::tools::QuicSimpleServer(config, net::QuicSupportedVersions());
  int rv = g_quic_server->Listen(net::IPEndPoint(ip, kServerPort));
  CHECK_GE(rv, 0) << "Quic server fails to start";
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_QuicTestServer_onServerStarted(env);
}

void ShutdownOnServerThread() {
  DCHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  g_quic_server->Shutdown();
  delete g_quic_server;
}

}  // namespace

// Quic server is currently hardcoded to run on port 6121 of the localhost on
// the device.
void StartQuicTestServer(JNIEnv* env,
                         jclass /*jcaller*/,
                         jstring jtest_files_root) {
  DCHECK(!g_quic_server_thread);
  g_quic_server_thread = new base::Thread("quic server thread");
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  bool started = g_quic_server_thread->StartWithOptions(thread_options);
  DCHECK(started);
  base::FilePath test_files_root(
      base::android::ConvertJavaStringToUTF8(env, jtest_files_root));
  g_quic_server_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ServeFilesFromDirectory, test_files_root));
}

void ShutdownQuicTestServer(JNIEnv* env, jclass /*jcaller*/) {
  DCHECK(!g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  g_quic_server_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ShutdownOnServerThread));
  delete g_quic_server_thread;
}

jstring GetServerHost(JNIEnv* env, jclass /*jcaller*/) {
  return base::android::ConvertUTF8ToJavaString(env, kServerHost).Release();
}

int GetServerPort(JNIEnv* env, jclass /*jcaller*/) {
  return kServerPort;
}

bool RegisterQuicTestServer(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
