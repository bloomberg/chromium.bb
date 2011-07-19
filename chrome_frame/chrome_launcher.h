// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_LAUNCHER_H_
#define CHROME_FRAME_CHROME_LAUNCHER_H_

#include <string>

// arraysize macro shamelessly stolen from base\basictypes.h
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

#define arraysize(array) (sizeof(ArraySizeHelper(array)))

namespace chrome_launcher {

// The base name of the chrome_launcher.exe file.
extern const wchar_t kLauncherExeBaseName[];

// Returns true if command_line contains only flags that we allow through.
// Returns false if command_line contains any unrecognized flags.
bool IsValidCommandLine(const wchar_t* command_line);

// Given a command-line without an initial program part, launch our associated
// chrome.exe with a sanitized version of that command line. Returns true iff
// successful.
bool SanitizeAndLaunchChrome(const wchar_t* command_line);

// Returns a pointer to the position in command_line the string right after the
// name of the executable. This is equivalent to the second element of the
// array returned by CommandLineToArgvW. Returns NULL if there are no further
// arguments.
const wchar_t* GetArgumentsStart(const wchar_t* command_line);

// Returns the full path to the Chrome executable.
bool GetChromeExecutablePath(std::wstring* chrome_path);

// Returns whether a given argument is considered a valid flag. Only accepts
// flags of the forms:
//   --foo
//   --foo=bar
bool IsValidArgument(const std::wstring& argument);

// Returns a string that is equivalent in input_str without any leading
// or trailing whitespace. Returns an empty string if input_str contained only
// whitespace.
std::wstring TrimWhiteSpace(const wchar_t* input_str);

}  // namespace chrome_launcher

#endif  // CHROME_FRAME_CHROME_LAUNCHER_H_
