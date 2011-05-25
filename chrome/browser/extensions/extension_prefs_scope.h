// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_SCOPE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_SCOPE_H_
#pragma once

// TODO(battre): get rid of this namespace, see
// http://codereview.chromium.org/7065033/diff/3003/chrome/browser/extensions/extension_prefs_scope.h
namespace extension_prefs_scope {

// Scope for a preference.
enum Scope {
  // Regular profile.
  kRegular,
  // Incognito profile; preference is persisted to disk and remains active
  // after a browser restart.
  kIncognitoPersistent,
  // TODO(battre): add kIncognitoSession
};

}  // extension_prefs_scope

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_SCOPE_H_
