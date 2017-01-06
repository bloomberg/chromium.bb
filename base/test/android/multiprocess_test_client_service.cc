// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/multiprocess_test_client_service.h"

#include "base/android/jni_array.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/posix/global_descriptors.h"
#include "jni/MultiprocessTestClientService_jni.h"

extern int main(int argc, char** argv);

namespace base {
namespace android {

bool RegisterMultiprocessTestClientServiceJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void RunMain(JNIEnv* env,
             const base::android::JavaParamRef<_jobject*>& jcaller,
             const base::android::JavaParamRef<jobjectArray>& command_line,
             const base::android::JavaParamRef<jintArray>& fds_to_map_keys,
             const base::android::JavaParamRef<jintArray>& fds_to_map_fds) {
  // Guards against process being reused.
  // Static variables, singletons, lazy instances make running code again in the
  // same child process difficult.
  static bool alreadyRun = false;
  CHECK(!alreadyRun);
  alreadyRun = true;

  std::vector<std::string> cpp_command_line;
  AppendJavaStringArrayToStringVector(env, command_line, &cpp_command_line);

  std::vector<int> keys;
  base::android::JavaIntArrayToIntVector(env, fds_to_map_keys, &keys);
  std::vector<int> fds;
  base::android::JavaIntArrayToIntVector(env, fds_to_map_fds, &fds);
  CHECK_EQ(keys.size(), fds.size());

  for (size_t i = 0; i < keys.size(); i++) {
    base::GlobalDescriptors::GetInstance()->Set(keys[i], fds[i]);
  }
  std::vector<char*> c_command_line;
  for (auto& entry : cpp_command_line) {
    c_command_line.push_back(&entry[0]);
  }

  int result = main(c_command_line.size(), &c_command_line[0]);

  Java_MultiprocessTestClientService_setMainReturnValue(env, jcaller, result);
}

}  // namespace android
}  // namespace base