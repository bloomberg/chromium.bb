// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_DELEGATE_H_
#pragma once

class AutocompleteControllerDelegate {
 public:
  // Invoked when the result set of the AutocompleteController changes. If
  // |default_match_changed| is true, the default match of the result set has
  // changed.
  virtual void OnResultChanged(bool default_match_changed) = 0;

 protected:
  virtual ~AutocompleteControllerDelegate() {}
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_DELEGATE_H_
