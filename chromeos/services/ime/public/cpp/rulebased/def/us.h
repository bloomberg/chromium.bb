// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_US_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_US_H_

// The key mapping definitions under various modifier states for the US layout.
extern const wchar_t* kKeyMapUs[];

// The length of the key mapping definitions.
extern const unsigned int kKeyMapUsLen;

// The indexes pointed to the key mapping definition under certain modifier
// state.
extern const unsigned char kKeyMapIndexUs[8];

// The Id for the US keyboard.
extern const char* kIdUs;

// Whether the US keyboard is 102 or 101 keyboard.
extern const bool kIs102Us;

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_DEF_US_H_
