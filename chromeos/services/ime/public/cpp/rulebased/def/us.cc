// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/def/us.h"

#include "base/stl_util.h"

const wchar_t* kKeyMapUs[] = {
    // Row #1
    L"`1234567890-="
    // Row #2
    L"qwertyuiop[]\\"
    // Row #3
    L"asdfghjkl;'"
    // Row #4
    L"zxcvbnm,./"
    // Row #5
    L"\u0020",
    // Row #1
    L"~!@#$%^&*()_+"
    // Row #2
    L"QWERTYUIOP{}|"
    // Row #3
    L"ASDFGHJKL:\""
    // Row #4
    L"ZXCVBNM<>?"
    // Row #5
    L"\u0020",
    // Row #1
    L"`1234567890-="
    // Row #2
    L"QWERTYUIOP[]\\"
    // Row #3
    L"ASDFGHJKL;'"
    // Row #4
    L"ZXCVBNM,./"
    // Row #5
    L"\u0020",
    // Row #1
    L"~!@#$%^&*()_+"
    // Row #2
    L"qwertyuiop{}|"
    // Row #3
    L"asdfghjkl:\""
    // Row #4
    L"zxcvbnm<>?"
    // Row #5
    L"\u0020"};
const unsigned int kKeyMapUsLen = base::size(kKeyMapUs);
const unsigned char kKeyMapIndexUs[8]{0, 1, 0, 1, 2, 3, 2, 3};
const char* kIdUs = "us";
const bool kIs102Us = false;
