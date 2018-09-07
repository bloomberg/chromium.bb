// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_auralinux.h"

#include "base/stl_util.h"

namespace content {
namespace {
struct PlatformConstantToNameEntry {
  int32_t value;
  const char* name;
};

const char* GetNameForPlatformConstant(
    const PlatformConstantToNameEntry table[],
    size_t table_size,
    int32_t value) {
  for (size_t i = 0; i < table_size; ++i) {
    auto& entry = table[i];
    if (entry.value == value)
      return entry.name;
  }
  return "<unknown>";
}
}  // namespace

#define QUOTE(X) \
  { X, #X }

CONTENT_EXPORT const char* ATSPIStateToString(AtspiStateType state) {
  static const PlatformConstantToNameEntry state_table[] = {
      QUOTE(ATSPI_STATE_INVALID),
      QUOTE(ATSPI_STATE_ACTIVE),
      QUOTE(ATSPI_STATE_ARMED),
      QUOTE(ATSPI_STATE_BUSY),
      QUOTE(ATSPI_STATE_CHECKED),
      QUOTE(ATSPI_STATE_COLLAPSED),
      QUOTE(ATSPI_STATE_DEFUNCT),
      QUOTE(ATSPI_STATE_EDITABLE),
      QUOTE(ATSPI_STATE_ENABLED),
      QUOTE(ATSPI_STATE_EXPANDABLE),
      QUOTE(ATSPI_STATE_EXPANDED),
      QUOTE(ATSPI_STATE_FOCUSABLE),
      QUOTE(ATSPI_STATE_FOCUSED),
      QUOTE(ATSPI_STATE_HAS_TOOLTIP),
      QUOTE(ATSPI_STATE_HORIZONTAL),
      QUOTE(ATSPI_STATE_ICONIFIED),
      QUOTE(ATSPI_STATE_MODAL),
      QUOTE(ATSPI_STATE_MULTI_LINE),
      QUOTE(ATSPI_STATE_MULTISELECTABLE),
      QUOTE(ATSPI_STATE_OPAQUE),
      QUOTE(ATSPI_STATE_PRESSED),
      QUOTE(ATSPI_STATE_RESIZABLE),
      QUOTE(ATSPI_STATE_SELECTABLE),
      QUOTE(ATSPI_STATE_SELECTED),
      QUOTE(ATSPI_STATE_SENSITIVE),
      QUOTE(ATSPI_STATE_SHOWING),
      QUOTE(ATSPI_STATE_SINGLE_LINE),
      QUOTE(ATSPI_STATE_STALE),
      QUOTE(ATSPI_STATE_TRANSIENT),
      QUOTE(ATSPI_STATE_VERTICAL),
      QUOTE(ATSPI_STATE_VISIBLE),
      QUOTE(ATSPI_STATE_MANAGES_DESCENDANTS),
      QUOTE(ATSPI_STATE_INDETERMINATE),
      QUOTE(ATSPI_STATE_REQUIRED),
      QUOTE(ATSPI_STATE_TRUNCATED),
      QUOTE(ATSPI_STATE_ANIMATED),
      QUOTE(ATSPI_STATE_INVALID_ENTRY),
      QUOTE(ATSPI_STATE_SUPPORTS_AUTOCOMPLETION),
      QUOTE(ATSPI_STATE_SELECTABLE_TEXT),
      QUOTE(ATSPI_STATE_IS_DEFAULT),
      QUOTE(ATSPI_STATE_VISITED),
      QUOTE(ATSPI_STATE_CHECKABLE),
      QUOTE(ATSPI_STATE_HAS_POPUP),
      QUOTE(ATSPI_STATE_READ_ONLY),
      QUOTE(ATSPI_STATE_LAST_DEFINED),
  };

  return GetNameForPlatformConstant(state_table, base::size(state_table),
                                    state);
}

}  // namespace content
