// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_MODEL_H_
#pragma once

class SkBitmap;

// An interface implemented by an object that provides data to populate
// individual result views.
class AutocompleteResultViewModel {
 public:
  // Returns true if the index is selected.
  virtual bool IsSelectedIndex(size_t index) const = 0;

  // Returns true if the index is hovered.
  virtual bool IsHoveredIndex(size_t index) const = 0;

  // If |index| is a match from an extension, returns the extension icon;
  // otherwise returns NULL.
  virtual const SkBitmap* GetIconIfExtensionMatch(size_t index) const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_VIEW_MODEL_H_
