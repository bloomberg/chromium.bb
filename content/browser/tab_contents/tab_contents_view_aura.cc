// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is just a placeholder - TabContentsViewAura is not yet implemented
// (instead chrome has it's own TabContentsViewViews that is used for Aura).

// We need to make sure the compiler sees this interface declaration somewhere
// so that it can emit exported versions of the inline ctor/dtor functions,
// otherwise chrome.dll will get a linker error on Windows Aura builds.
#include "content/public/browser/web_contents_view_delegate.h"
