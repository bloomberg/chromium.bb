// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
#define CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_

#include <jni.h>

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

// Registers the callbacks that allows the entry point of the library to be
// exposed to the calling java code.  This handles only registering the content
// specific callbacks.  Any application specific JNI bindings should happen
// once the native library has fully loaded.
CONTENT_EXPORT bool RegisterLibraryLoaderEntryHook(JNIEnv* env);

// Register all content JNI functions now, rather than waiting for the process
// of fully loading the native library to complete.  This must only be called
// during JNI_OnLoad.
CONTENT_EXPORT bool EnsureJniRegistered(JNIEnv* env);

// Pass the version name to Content. This used to check that the library version
// matches the version expected by Java before completing JNI registration.
// Note: argument must remain valid at least until library loading is complete.
CONTENT_EXPORT void SetVersionNumber(const char* version_number);

// Call on exit to delete the AtExitManager which OnLibraryLoadedOnUIThread
// created.
CONTENT_EXPORT void LibraryLoaderExitHook();

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_ANDROID_LIBRARY_LOADER_HOOKS_H_
