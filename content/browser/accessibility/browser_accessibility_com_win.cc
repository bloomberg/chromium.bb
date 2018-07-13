// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_com_win.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/enum_variant.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/common/accessibility_messages.h"
#include "content/public/common/content_client.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_modes.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_text_utils.h"
#include "ui/base/win/accessibility_ids_win.h"
#include "ui/base/win/accessibility_misc_utils.h"
#include "ui/base/win/atl_module.h"

// There is no easy way to decouple |kScreenReader| and |kHTML| accessibility
// modes when Windows screen readers are used. For example, certain roles use
// the HTML tag name. Input fields require their type attribute to be exposed.
const uint32_t kScreenReaderAndHTMLAccessibilityModes =
    ui::AXMode::kScreenReader | ui::AXMode::kHTML;

namespace content {

using BrowserAccessibilityPositionInstance =
    BrowserAccessibilityPosition::AXPositionInstance;
using AXPlatformRange =
    ui::AXRange<BrowserAccessibilityPositionInstance::element_type>;

void AddAccessibilityModeFlags(ui::AXMode mode_flags) {
  BrowserAccessibilityStateImpl::GetInstance()->AddAccessibilityModeFlags(
      mode_flags);
}

//
// BrowserAccessibilityComWin::WinAttributes
//

BrowserAccessibilityComWin::WinAttributes::WinAttributes()
    : ia_role(0), ia_state(0), ia2_role(0), ia2_state(0) {}

BrowserAccessibilityComWin::WinAttributes::~WinAttributes() {}

//
// BrowserAccessibilityComWin
//
BrowserAccessibilityComWin::BrowserAccessibilityComWin()
    : owner_(nullptr),
      win_attributes_(new WinAttributes()),
      previous_scroll_x_(0),
      previous_scroll_y_(0) {}

BrowserAccessibilityComWin::~BrowserAccessibilityComWin() {
}

//
// IAccessible2 overrides:
//

STDMETHODIMP BrowserAccessibilityComWin::get_attributes(BSTR* attributes) {
  // This can be removed once ISimpleDOMNode is migrated
  return AXPlatformNodeWin::get_attributes(attributes);
}

STDMETHODIMP BrowserAccessibilityComWin::scrollTo(IA2ScrollType scroll_type) {
  // This can be removed once ISimpleDOMNode is migrated
  return AXPlatformNodeWin::scrollTo(scroll_type);
}

//
// IAccessibleApplication methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_appName(BSTR* app_name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_APP_NAME);

  if (!app_name)
    return E_INVALIDARG;

  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part before the "/".
  std::vector<std::string> product_components =
      base::SplitString(GetContentClient()->GetProduct(), "/",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(2U, product_components.size());
  if (product_components.size() != 2)
    return E_FAIL;
  *app_name = SysAllocString(base::UTF8ToUTF16(product_components[0]).c_str());
  DCHECK(*app_name);
  return *app_name ? S_OK : E_FAIL;
}

STDMETHODIMP BrowserAccessibilityComWin::get_appVersion(BSTR* app_version) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_APP_VERSION);

  if (!app_version)
    return E_INVALIDARG;

  // GetProduct() returns a string like "Chrome/aa.bb.cc.dd", split out
  // the part after the "/".
  std::vector<std::string> product_components =
      base::SplitString(GetContentClient()->GetProduct(), "/",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_EQ(2U, product_components.size());
  if (product_components.size() != 2)
    return E_FAIL;
  *app_version =
      SysAllocString(base::UTF8ToUTF16(product_components[1]).c_str());
  DCHECK(*app_version);
  return *app_version ? S_OK : E_FAIL;
}

STDMETHODIMP BrowserAccessibilityComWin::get_toolkitName(BSTR* toolkit_name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TOOLKIT_NAME);
  if (!toolkit_name)
    return E_INVALIDARG;

  // This is hard-coded; all products based on the Chromium engine
  // will have the same toolkit name, so that assistive technology can
  // detect any Chrome-based product.
  *toolkit_name = SysAllocString(L"Chrome");
  DCHECK(*toolkit_name);
  return *toolkit_name ? S_OK : E_FAIL;
}

STDMETHODIMP BrowserAccessibilityComWin::get_toolkitVersion(
    BSTR* toolkit_version) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TOOLKIT_VERSION);
  if (!toolkit_version)
    return E_INVALIDARG;

  std::string user_agent = GetContentClient()->GetUserAgent();
  *toolkit_version = SysAllocString(base::UTF8ToUTF16(user_agent).c_str());
  DCHECK(*toolkit_version);
  return *toolkit_version ? S_OK : E_FAIL;
}

//
// IAccessibleImage methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_description(BSTR* desc) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_DESCRIPTION);
  if (!owner())
    return E_FAIL;

  if (!desc)
    return E_INVALIDARG;

  if (description().empty())
    return S_FALSE;

  *desc = SysAllocString(description().c_str());

  DCHECK(*desc);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_imagePosition(
    IA2CoordinateType coordinate_type,
    LONG* x,
    LONG* y) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_IMAGE_POSITION);
  if (!owner())
    return E_FAIL;

  if (!x || !y)
    return E_INVALIDARG;

  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    gfx::Rect bounds = owner()->GetUnclippedScreenBoundsRect();
    *x = bounds.x();
    *y = bounds.y();
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    gfx::Rect bounds = owner()->GetPageBoundsRect();
    gfx::Rect parent_bounds =
        owner()->PlatformGetParent()
            ? owner()->PlatformGetParent()->GetPageBoundsRect()
            : gfx::Rect();
    *x = bounds.x() - parent_bounds.x();
    *y = bounds.y() - parent_bounds.y();
  } else {
    return E_INVALIDARG;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_imageSize(LONG* height,
                                                       LONG* width) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_IMAGE_SIZE);
  if (!owner())
    return E_FAIL;

  if (!height || !width)
    return E_INVALIDARG;

  *height = owner()->GetPageBoundsRect().height();
  *width = owner()->GetPageBoundsRect().width();
  return S_OK;
}

//
// IAccessibleText methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_nCharacters(LONG* n_characters) {
  return AXPlatformNodeWin::get_nCharacters(n_characters);
}

STDMETHODIMP BrowserAccessibilityComWin::get_caretOffset(LONG* offset) {
  return AXPlatformNodeWin::get_caretOffset(offset);
}

STDMETHODIMP BrowserAccessibilityComWin::get_characterExtents(
    LONG offset,
    IA2CoordinateType coordinate_type,
    LONG* out_x,
    LONG* out_y,
    LONG* out_width,
    LONG* out_height) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CHARACTER_EXTENTS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!out_x || !out_y || !out_width || !out_height)
    return E_INVALIDARG;

  const base::string16& text_str = GetText();
  HandleSpecialTextOffset(&offset);
  if (offset < 0 || offset > static_cast<LONG>(text_str.size()))
    return E_INVALIDARG;

  gfx::Rect character_bounds;
  if (coordinate_type == IA2_COORDTYPE_SCREEN_RELATIVE) {
    character_bounds = owner()->GetScreenBoundsForRange(offset, 1);
  } else if (coordinate_type == IA2_COORDTYPE_PARENT_RELATIVE) {
    character_bounds = owner()->GetPageBoundsForRange(offset, 1);
    if (owner()->PlatformGetParent()) {
      character_bounds -=
          owner()->PlatformGetParent()->GetPageBoundsRect().OffsetFromOrigin();
    }
  } else {
    return E_INVALIDARG;
  }

  *out_x = character_bounds.x();
  *out_y = character_bounds.y();
  *out_width = character_bounds.width();
  *out_height = character_bounds.height();

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_nSelections(LONG* n_selections) {
  return AXPlatformNodeWin::get_nSelections(n_selections);
}

STDMETHODIMP BrowserAccessibilityComWin::get_selection(LONG selection_index,
                                                       LONG* start_offset,
                                                       LONG* end_offset) {
  return AXPlatformNodeWin::get_selection(selection_index, start_offset,
                                          end_offset);
}

STDMETHODIMP BrowserAccessibilityComWin::get_text(LONG start_offset,
                                                  LONG end_offset,
                                                  BSTR* text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!text)
    return E_INVALIDARG;

  const base::string16& text_str = GetText();
  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);

  // The spec allows the arguments to be reversed.
  if (start_offset > end_offset)
    std::swap(start_offset, end_offset);

  LONG len = static_cast<LONG>(text_str.length());
  if (start_offset < 0 || start_offset > len)
    return E_INVALIDARG;
  if (end_offset < 0 || end_offset > len)
    return E_INVALIDARG;

  base::string16 substr =
      text_str.substr(start_offset, end_offset - start_offset);
  if (substr.empty())
    return S_FALSE;

  *text = SysAllocString(substr.c_str());
  DCHECK(*text);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_textAtOffset(
    LONG offset,
    IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT_AT_OFFSET);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  *start_offset = 0;
  *end_offset = 0;
  *text = nullptr;

  HandleSpecialTextOffset(&offset);
  if (offset < 0)
    return E_INVALIDARG;

  const base::string16& text_str = GetText();
  LONG text_len = text_str.length();
  if (offset > text_len)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE)
    return S_FALSE;

  // According to the IA2 Spec, only line boundaries should succeed when
  // the offset is one past the end of the text.
  if (offset == text_len && boundary_type != IA2_TEXT_BOUNDARY_LINE)
    return S_FALSE;

  LONG start = FindBoundary(boundary_type, offset, ui::BACKWARDS_DIRECTION);
  LONG end = FindBoundary(boundary_type, start, ui::FORWARDS_DIRECTION);
  if (end < offset)
    return S_FALSE;

  *start_offset = start;
  *end_offset = end;
  return get_text(start, end, text);
}

STDMETHODIMP BrowserAccessibilityComWin::get_textBeforeOffset(
    LONG offset,
    IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT_BEFORE_OFFSET);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  *start_offset = 0;
  *end_offset = 0;
  *text = NULL;

  const base::string16& text_str = GetText();
  LONG text_len = text_str.length();
  if (offset > text_len)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE)
    return S_FALSE;

  *start_offset = FindBoundary(boundary_type, offset, ui::BACKWARDS_DIRECTION);
  *end_offset = offset;
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityComWin::get_textAfterOffset(
    LONG offset,
    IA2TextBoundaryType boundary_type,
    LONG* start_offset,
    LONG* end_offset,
    BSTR* text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TEXT_AFTER_OFFSET);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!start_offset || !end_offset || !text)
    return E_INVALIDARG;

  *start_offset = 0;
  *end_offset = 0;
  *text = NULL;

  const base::string16& text_str = GetText();
  LONG text_len = text_str.length();
  if (offset > text_len)
    return E_INVALIDARG;

  // The IAccessible2 spec says we don't have to implement the "sentence"
  // boundary type, we can just let the screenreader handle it.
  if (boundary_type == IA2_TEXT_BOUNDARY_SENTENCE)
    return S_FALSE;

  *start_offset = offset;
  *end_offset = FindBoundary(boundary_type, offset, ui::FORWARDS_DIRECTION);
  return get_text(*start_offset, *end_offset, text);
}

STDMETHODIMP BrowserAccessibilityComWin::get_newText(IA2TextSegment* new_text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NEW_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!new_text)
    return E_INVALIDARG;

  if (!old_win_attributes_)
    return E_FAIL;

  int start, old_len, new_len;
  ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
  if (new_len == 0)
    return E_FAIL;

  base::string16 substr = GetText().substr(start, new_len);
  new_text->text = SysAllocString(substr.c_str());
  new_text->start = static_cast<long>(start);
  new_text->end = static_cast<long>(start + new_len);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_oldText(IA2TextSegment* old_text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_OLD_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!old_text)
    return E_INVALIDARG;

  if (!old_win_attributes_)
    return E_FAIL;

  int start, old_len, new_len;
  ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
  if (old_len == 0)
    return E_FAIL;

  base::string16 old_hypertext = old_hypertext_.hypertext;
  base::string16 substr = old_hypertext.substr(start, old_len);
  old_text->text = SysAllocString(substr.c_str());
  old_text->start = static_cast<long>(start);
  old_text->end = static_cast<long>(start + old_len);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_offsetAtPoint(
    LONG x,
    LONG y,
    IA2CoordinateType coord_type,
    LONG* offset) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_OFFSET_AT_POINT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!offset)
    return E_INVALIDARG;

  // TODO(dmazzoni): implement this. We're returning S_OK for now so that
  // screen readers still return partially accurate results rather than
  // completely failing.
  *offset = 0;
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::scrollSubstringTo(
    LONG start_index,
    LONG end_index,
    IA2ScrollType scroll_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_SUBSTRING_TO);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  // TODO(dmazzoni): adjust this for the start and end index, too.
  return scrollTo(scroll_type);
}

STDMETHODIMP BrowserAccessibilityComWin::scrollSubstringToPoint(
    LONG start_index,
    LONG end_index,
    IA2CoordinateType coordinate_type,
    LONG x,
    LONG y) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_SUBSTRING_TO_POINT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (start_index > end_index)
    std::swap(start_index, end_index);
  LONG length = end_index - start_index + 1;
  DCHECK_GE(length, 0);

  gfx::Rect string_bounds = owner()->GetPageBoundsForRange(start_index, length);
  string_bounds -= owner()->GetPageBoundsRect().OffsetFromOrigin();
  x -= string_bounds.x();
  y -= string_bounds.y();

  return scrollToPoint(coordinate_type, x, y);
}

STDMETHODIMP BrowserAccessibilityComWin::addSelection(LONG start_offset,
                                                      LONG end_offset) {
  return AXPlatformNodeWin::addSelection(start_offset, end_offset);
}

STDMETHODIMP BrowserAccessibilityComWin::removeSelection(LONG selection_index) {
  return AXPlatformNodeWin::removeSelection(selection_index);
}

STDMETHODIMP BrowserAccessibilityComWin::setCaretOffset(LONG offset) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SET_CARET_OFFSET);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;
  SetIA2HypertextSelection(offset, offset);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::setSelection(LONG selection_index,
                                                      LONG start_offset,
                                                      LONG end_offset) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SET_SELECTION);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;
  if (selection_index != 0)
    return E_INVALIDARG;
  SetIA2HypertextSelection(start_offset, end_offset);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_attributes(LONG offset,
                                                        LONG* start_offset,
                                                        LONG* end_offset,
                                                        BSTR* text_attributes) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IATEXT_GET_ATTRIBUTES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!start_offset || !end_offset || !text_attributes)
    return E_INVALIDARG;

  *start_offset = *end_offset = 0;
  *text_attributes = nullptr;
  if (!owner())
    return E_FAIL;

  const base::string16 text = GetText();
  HandleSpecialTextOffset(&offset);
  if (offset < 0 || offset > static_cast<LONG>(text.size()))
    return E_INVALIDARG;

  ComputeStylesIfNeeded();
  *start_offset = FindStartOfStyle(offset, ui::BACKWARDS_DIRECTION);
  *end_offset = FindStartOfStyle(offset, ui::FORWARDS_DIRECTION);

  base::string16 attributes_str;
  const std::vector<base::string16>& attributes =
      offset_to_text_attributes().find(*start_offset)->second;
  for (const base::string16& attribute : attributes) {
    attributes_str += attribute + L';';
  }

  if (attributes.empty())
    return S_FALSE;

  *text_attributes = SysAllocString(attributes_str.c_str());
  DCHECK(*text_attributes);
  return S_OK;
}

//
// IAccessibleHypertext methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_nHyperlinks(
    long* hyperlink_count) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_N_HYPERLINKS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!hyperlink_count)
    return E_INVALIDARG;

  *hyperlink_count = hypertext_.hyperlink_offset_to_index.size();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_hyperlink(
    long index,
    IAccessibleHyperlink** hyperlink) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_HYPERLINK);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!hyperlink || index < 0 ||
      index >= static_cast<long>(hypertext_.hyperlinks.size())) {
    return E_INVALIDARG;
  }

  int32_t id = hypertext_.hyperlinks[index];
  auto* link = static_cast<BrowserAccessibilityComWin*>(
      AXPlatformNodeWin::GetFromUniqueId(id));
  if (!link)
    return E_FAIL;

  *hyperlink = static_cast<IAccessibleHyperlink*>(link->NewReference());
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_hyperlinkIndex(
    long char_index,
    long* hyperlink_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_HYPERLINK_INDEX);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!hyperlink_index)
    return E_INVALIDARG;

  if (char_index < 0 || char_index >= static_cast<long>(GetText().size())) {
    return E_INVALIDARG;
  }

  std::map<int32_t, int32_t>::iterator it =
      hypertext_.hyperlink_offset_to_index.find(char_index);
  if (it == hypertext_.hyperlink_offset_to_index.end()) {
    *hyperlink_index = -1;
    return S_FALSE;
  }

  *hyperlink_index = it->second;
  return S_OK;
}

//
// IAccessibleHyperlink methods.
//

// Currently, only text links are supported.
STDMETHODIMP BrowserAccessibilityComWin::get_anchor(long index,
                                                    VARIANT* anchor) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ANCHOR);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner() || !IsHyperlink())
    return E_FAIL;

  // IA2 text links can have only one anchor, that is the text inside them.
  if (index != 0 || !anchor)
    return E_INVALIDARG;

  BSTR ia2_hypertext = SysAllocString(GetText().c_str());
  DCHECK(ia2_hypertext);
  anchor->vt = VT_BSTR;
  anchor->bstrVal = ia2_hypertext;

  // Returning S_FALSE is not mentioned in the IA2 Spec, but it might have been
  // an oversight.
  if (!SysStringLen(ia2_hypertext))
    return S_FALSE;

  return S_OK;
}

// Currently, only text links are supported.
STDMETHODIMP BrowserAccessibilityComWin::get_anchorTarget(
    long index,
    VARIANT* anchor_target) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ANCHOR_TARGET);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner() || !IsHyperlink())
    return E_FAIL;

  // IA2 text links can have at most one target, that is when they represent an
  // HTML hyperlink, i.e. an <a> element with a "href" attribute.
  if (index != 0 || !anchor_target)
    return E_INVALIDARG;

  BSTR target;
  if (!(MSAAState() & STATE_SYSTEM_LINKED) ||
      FAILED(GetStringAttributeAsBstr(ax::mojom::StringAttribute::kUrl,
                                      &target))) {
    target = SysAllocString(L"");
  }
  DCHECK(target);
  anchor_target->vt = VT_BSTR;
  anchor_target->bstrVal = target;

  // Returning S_FALSE is not mentioned in the IA2 Spec, but it might have been
  // an oversight.
  if (!SysStringLen(target))
    return S_FALSE;

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_startIndex(long* index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_START_INDEX);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner() || !IsHyperlink())
    return E_FAIL;

  if (!index)
    return E_INVALIDARG;

  int32_t hypertext_offset = 0;
  auto* parent = owner()->PlatformGetParent();
  if (parent) {
    hypertext_offset =
        ToBrowserAccessibilityComWin(parent)->GetHypertextOffsetFromChild(this);
  }
  *index = static_cast<LONG>(hypertext_offset);
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_endIndex(long* index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_END_INDEX);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  LONG start_index;
  HRESULT hr = get_startIndex(&start_index);
  if (hr == S_OK)
    *index = start_index + 1;
  return hr;
}

// This method is deprecated in the IA2 Spec.
STDMETHODIMP BrowserAccessibilityComWin::get_valid(boolean* valid) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_VALID);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  return E_NOTIMPL;
}

//
// IAccessibleAction partly implemented.
//

STDMETHODIMP BrowserAccessibilityComWin::nActions(long* n_actions) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_N_ACTIONS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!n_actions)
    return E_INVALIDARG;

  // |IsHyperlink| is required for |IAccessibleHyperlink::anchor/anchorTarget|
  // to work properly because the |IAccessibleHyperlink| interface inherits from
  // |IAccessibleAction|.
  if (IsHyperlink() ||
      owner()->HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb)) {
    *n_actions = 1;
  } else {
    *n_actions = 0;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::doAction(long action_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_DO_ACTION);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!owner()->HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb) ||
      action_index != 0)
    return E_INVALIDARG;

  Manager()->DoDefaultAction(*owner());
  return S_OK;
}

STDMETHODIMP
BrowserAccessibilityComWin::get_description(long action_index,
                                            BSTR* description) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_IAACTION_GET_DESCRIPTION);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityComWin::get_keyBinding(long action_index,
                                                        long n_max_bindings,
                                                        BSTR** key_bindings,
                                                        long* n_bindings) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_KEY_BINDING);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityComWin::get_name(long action_index,
                                                  BSTR* name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NAME);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!name)
    return E_INVALIDARG;

  int action;
  if (!owner()->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                                &action) ||
      action_index != 0) {
    *name = nullptr;
    return E_INVALIDARG;
  }

  base::string16 action_verb = ui::ActionVerbToUnlocalizedString(
      static_cast<ax::mojom::DefaultActionVerb>(action));
  if (action_verb.empty() || action_verb == L"none") {
    *name = nullptr;
    return S_FALSE;
  }

  *name = SysAllocString(action_verb.c_str());
  DCHECK(name);
  return S_OK;
}

STDMETHODIMP
BrowserAccessibilityComWin::get_localizedName(long action_index,
                                              BSTR* localized_name) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCALIZED_NAME);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!localized_name)
    return E_INVALIDARG;

  int action;
  if (!owner()->GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb,
                                &action) ||
      action_index != 0) {
    *localized_name = nullptr;
    return E_INVALIDARG;
  }

  base::string16 action_verb = ui::ActionVerbToLocalizedString(
      static_cast<ax::mojom::DefaultActionVerb>(action));
  if (action_verb.empty()) {
    *localized_name = nullptr;
    return S_FALSE;
  }

  *localized_name = SysAllocString(action_verb.c_str());
  DCHECK(localized_name);
  return S_OK;
}

//
// IAccessibleValue methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_currentValue(VARIANT* value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CURRENT_VALUE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityComWin::get_minimumValue(VARIANT* value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_MINIMUM_VALUE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityComWin::get_maximumValue(VARIANT* value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_MAXIMUM_VALUE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!value)
    return E_INVALIDARG;

  float float_val;
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange,
                        &float_val)) {
    value->vt = VT_R8;
    value->dblVal = float_val;
    return S_OK;
  }

  value->vt = VT_EMPTY;
  return S_FALSE;
}

STDMETHODIMP BrowserAccessibilityComWin::setCurrentValue(VARIANT new_value) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SET_CURRENT_VALUE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  // TODO(dmazzoni): Implement this.
  return E_NOTIMPL;
}

//
// ISimpleDOMDocument methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_URL(BSTR* url) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_URL);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!url)
    return E_INVALIDARG;

  if (owner() != manager->GetRoot())
    return E_FAIL;

  std::string str = manager->GetTreeData().url;
  if (str.empty())
    return S_FALSE;

  *url = SysAllocString(base::UTF8ToUTF16(str).c_str());
  DCHECK(*url);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_title(BSTR* title) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_TITLE);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!title)
    return E_INVALIDARG;

  std::string str = manager->GetTreeData().title;
  if (str.empty())
    return S_FALSE;

  *title = SysAllocString(base::UTF8ToUTF16(str).c_str());
  DCHECK(*title);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_mimeType(BSTR* mime_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_MIME_TYPE);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!mime_type)
    return E_INVALIDARG;

  std::string str = manager->GetTreeData().mimetype;
  if (str.empty())
    return S_FALSE;

  *mime_type = SysAllocString(base::UTF8ToUTF16(str).c_str());
  DCHECK(*mime_type);

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_docType(BSTR* doc_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_DOC_TYPE);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  if (!doc_type)
    return E_INVALIDARG;

  std::string str = manager->GetTreeData().doctype;
  if (str.empty())
    return S_FALSE;

  *doc_type = SysAllocString(base::UTF8ToUTF16(str).c_str());
  DCHECK(*doc_type);

  return S_OK;
}

STDMETHODIMP
BrowserAccessibilityComWin::get_nameSpaceURIForID(short name_space_id,
                                                  BSTR* name_space_uri) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NAMESPACE_URI_FOR_ID);
  return E_NOTIMPL;
}

STDMETHODIMP
BrowserAccessibilityComWin::put_alternateViewMediaTypes(
    BSTR* comma_separated_media_types) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_PUT_ALTERNATE_VIEW_MEDIA_TYPES);
  return E_NOTIMPL;
}

//
// ISimpleDOMNode methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_nodeInfo(
    BSTR* node_name,
    short* name_space_id,
    BSTR* node_value,
    unsigned int* num_children,
    unsigned int* unique_id,
    unsigned short* node_type) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NODE_INFO);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!node_name || !name_space_id || !node_value || !num_children ||
      !unique_id || !node_type) {
    return E_INVALIDARG;
  }

  base::string16 tag;
  if (owner()->GetString16Attribute(ax::mojom::StringAttribute::kHtmlTag, &tag))
    *node_name = SysAllocString(tag.c_str());
  else
    *node_name = nullptr;

  *name_space_id = 0;
  *node_value = SysAllocString(value().c_str());
  *num_children = owner()->PlatformChildCount();
  *unique_id = -AXPlatformNodeWin::GetUniqueId();

  if (owner()->IsDocument()) {
    *node_type = NODETYPE_DOCUMENT;
  } else if (owner()->IsTextOnlyObject()) {
    *node_type = NODETYPE_TEXT;
  } else {
    *node_type = NODETYPE_ELEMENT;
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_attributes(
    unsigned short max_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values,
    unsigned short* num_attribs) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ISIMPLEDOMNODE_GET_ATTRIBUTES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values || !num_attribs)
    return E_INVALIDARG;

  *num_attribs = max_attribs;
  if (*num_attribs > owner()->GetHtmlAttributes().size())
    *num_attribs = owner()->GetHtmlAttributes().size();

  for (unsigned short i = 0; i < *num_attribs; ++i) {
    attrib_names[i] = SysAllocString(
        base::UTF8ToUTF16(owner()->GetHtmlAttributes()[i].first).c_str());
    name_space_id[i] = 0;
    attrib_values[i] = SysAllocString(
        base::UTF8ToUTF16(owner()->GetHtmlAttributes()[i].second).c_str());
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_attributesForNames(
    unsigned short num_attribs,
    BSTR* attrib_names,
    short* name_space_id,
    BSTR* attrib_values) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_ATTRIBUTES_FOR_NAMES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!attrib_names || !name_space_id || !attrib_values)
    return E_INVALIDARG;

  for (unsigned short i = 0; i < num_attribs; ++i) {
    name_space_id[i] = 0;
    bool found = false;
    std::string name = base::UTF16ToUTF8((LPCWSTR)attrib_names[i]);
    for (unsigned int j = 0; j < owner()->GetHtmlAttributes().size(); ++j) {
      if (owner()->GetHtmlAttributes()[j].first == name) {
        attrib_values[i] = SysAllocString(
            base::UTF8ToUTF16(owner()->GetHtmlAttributes()[j].second).c_str());
        found = true;
        break;
      }
    }
    if (!found) {
      attrib_values[i] = NULL;
    }
  }
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_computedStyle(
    unsigned short max_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values,
    unsigned short* num_style_properties) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COMPUTED_STYLE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  base::string16 display;
  if (max_style_properties == 0 ||
      !owner()->GetString16Attribute(ax::mojom::StringAttribute::kDisplay,
                                     &display)) {
    *num_style_properties = 0;
    return S_OK;
  }

  *num_style_properties = 1;
  style_properties[0] = SysAllocString(L"display");
  style_values[0] = SysAllocString(display.c_str());

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_computedStyleForProperties(
    unsigned short num_style_properties,
    boolean use_alternate_view,
    BSTR* style_properties,
    BSTR* style_values) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_COMPUTED_STYLE_FOR_PROPERTIES);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!style_properties || !style_values)
    return E_INVALIDARG;

  // We only cache a single style property for now: DISPLAY

  for (unsigned short i = 0; i < num_style_properties; ++i) {
    base::string16 name = base::ToLowerASCII(
        reinterpret_cast<const base::char16*>(style_properties[i]));
    if (name == L"display") {
      base::string16 display =
          owner()->GetString16Attribute(ax::mojom::StringAttribute::kDisplay);
      style_values[i] = SysAllocString(display.c_str());
    } else {
      style_values[i] = NULL;
    }
  }

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::scrollTo(boolean placeTopLeft) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_ISIMPLEDOMNODE_SCROLL_TO);
  return scrollTo(placeTopLeft ? IA2_SCROLL_TYPE_TOP_LEFT
                               : IA2_SCROLL_TYPE_ANYWHERE);
}

STDMETHODIMP BrowserAccessibilityComWin::get_parentNode(ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PARENT_NODE);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  *node = ToBrowserAccessibilityComWin(owner()->PlatformGetParent())
              ->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_firstChild(ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_FIRST_CHILD);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (owner()->PlatformChildCount() == 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(owner()->PlatformGetChild(0))
              ->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_lastChild(ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LAST_CHILD);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (owner()->PlatformChildCount() == 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(
              owner()->PlatformGetChild(owner()->PlatformChildCount() - 1))
              ->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_previousSibling(
    ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_PREVIOUS_SIBLING);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (!owner()->PlatformGetParent() || GetIndexInParent() <= 0) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(
              owner()->PlatformGetParent()->InternalGetChild(
                  GetIndexInParent() - 1))
              ->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_nextSibling(
    ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_NEXT_SIBLING);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (!owner()->PlatformGetParent() || GetIndexInParent() < 0 ||
      GetIndexInParent() >=
          static_cast<int>(owner()->PlatformGetParent()->InternalChildCount()) -
              1) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(
              owner()->PlatformGetParent()->InternalGetChild(
                  GetIndexInParent() + 1))
              ->NewReference();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_childAt(unsigned int child_index,
                                                     ISimpleDOMNode** node) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CHILD_AT);
  if (!owner())
    return E_FAIL;

  if (!node)
    return E_INVALIDARG;

  if (child_index >= owner()->PlatformChildCount())
    return E_INVALIDARG;

  BrowserAccessibility* child = owner()->PlatformGetChild(child_index);
  if (!child) {
    *node = NULL;
    return S_FALSE;
  }

  *node = ToBrowserAccessibilityComWin(child)->NewReference();
  return S_OK;
}

// We only support this method for retrieving MathML content.
STDMETHODIMP BrowserAccessibilityComWin::get_innerHTML(BSTR* innerHTML) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_INNER_HTML);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;
  if (owner()->GetRole() != ax::mojom::Role::kMath)
    return E_NOTIMPL;

  base::string16 inner_html =
      owner()->GetString16Attribute(ax::mojom::StringAttribute::kInnerHtml);
  *innerHTML = SysAllocString(inner_html.c_str());
  DCHECK(*innerHTML);
  return S_OK;
}

STDMETHODIMP
BrowserAccessibilityComWin::get_localInterface(void** local_interface) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LOCAL_INTERFACE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  return E_NOTIMPL;
}

STDMETHODIMP BrowserAccessibilityComWin::get_language(BSTR* language) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_LANGUAGE);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!language)
    return E_INVALIDARG;
  *language = nullptr;

  if (!owner())
    return E_FAIL;

  base::string16 lang = owner()->GetInheritedString16Attribute(
      ax::mojom::StringAttribute::kLanguage);
  if (lang.empty())
    lang = L"en-US";

  *language = SysAllocString(lang.c_str());
  DCHECK(*language);
  return S_OK;
}

//
// ISimpleDOMText methods.
//

STDMETHODIMP BrowserAccessibilityComWin::get_domText(BSTR* dom_text) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_DOM_TEXT);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!owner())
    return E_FAIL;

  if (!dom_text)
    return E_INVALIDARG;

  return GetStringAttributeAsBstr(ax::mojom::StringAttribute::kName, dom_text);
}

STDMETHODIMP BrowserAccessibilityComWin::get_clippedSubstringBounds(
    unsigned int start_index,
    unsigned int end_index,
    int* out_x,
    int* out_y,
    int* out_width,
    int* out_height) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_CLIPPED_SUBSTRING_BOUNDS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  // TODO(dmazzoni): fully support this API by intersecting the
  // rect with the container's rect.
  return get_unclippedSubstringBounds(start_index, end_index, out_x, out_y,
                                      out_width, out_height);
}

STDMETHODIMP BrowserAccessibilityComWin::get_unclippedSubstringBounds(
    unsigned int start_index,
    unsigned int end_index,
    int* out_x,
    int* out_y,
    int* out_width,
    int* out_height) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_UNCLIPPED_SUBSTRING_BOUNDS);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  if (!out_x || !out_y || !out_width || !out_height)
    return E_INVALIDARG;

  unsigned int text_length = static_cast<unsigned int>(GetText().size());
  if (start_index > text_length || end_index > text_length ||
      start_index > end_index) {
    return E_INVALIDARG;
  }

  gfx::Rect bounds =
      owner()->GetScreenBoundsForRange(start_index, end_index - start_index);
  *out_x = bounds.x();
  *out_y = bounds.y();
  *out_width = bounds.width();
  *out_height = bounds.height();
  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::scrollToSubstring(
    unsigned int start_index,
    unsigned int end_index) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_SCROLL_TO_SUBSTRING);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes |
                            ui::AXMode::kInlineTextBoxes);
  if (!owner())
    return E_FAIL;

  auto* manager = Manager();
  if (!manager)
    return E_FAIL;

  unsigned int text_length = static_cast<unsigned int>(GetText().size());
  if (start_index > text_length || end_index > text_length ||
      start_index > end_index) {
    return E_INVALIDARG;
  }

  manager->ScrollToMakeVisible(
      *owner(),
      owner()->GetPageBoundsForRange(start_index, end_index - start_index));

  return S_OK;
}

STDMETHODIMP BrowserAccessibilityComWin::get_fontFamily(BSTR* font_family) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_GET_FONT_FAMILY);
  AddAccessibilityModeFlags(kScreenReaderAndHTMLAccessibilityModes);
  if (!font_family)
    return E_INVALIDARG;
  *font_family = nullptr;

  if (!owner())
    return E_FAIL;

  base::string16 family = owner()->GetInheritedString16Attribute(
      ax::mojom::StringAttribute::kFontFamily);
  if (family.empty())
    return S_FALSE;

  *font_family = SysAllocString(family.c_str());
  DCHECK(*font_family);
  return S_OK;
}

//
// IServiceProvider methods.
//

STDMETHODIMP BrowserAccessibilityComWin::QueryService(REFGUID guid_service,
                                                      REFIID riid,
                                                      void** object) {
  WIN_ACCESSIBILITY_API_HISTOGRAM(UMA_API_QUERY_SERVICE);
  if (!owner())
    return E_FAIL;

  if (guid_service == GUID_IAccessibleContentDocument) {
    // Special Mozilla extension: return the accessible for the root document.
    // Screen readers use this to distinguish between a document loaded event
    // on the root document vs on an iframe.
    BrowserAccessibility* node = owner();
    while (node->PlatformGetParent())
      node = node->PlatformGetParent()->manager()->GetRoot();
    return ToBrowserAccessibilityComWin(node)->QueryInterface(IID_IAccessible2,
                                                              object);
  }

  if (guid_service == IID_IAccessible || guid_service == IID_IAccessible2 ||
      guid_service == IID_IAccessibleAction ||
      guid_service == IID_IAccessibleApplication ||
      guid_service == IID_IAccessibleHyperlink ||
      guid_service == IID_IAccessibleHypertext ||
      guid_service == IID_IAccessibleImage ||
      guid_service == IID_IAccessibleTable ||
      guid_service == IID_IAccessibleTable2 ||
      guid_service == IID_IAccessibleTableCell ||
      guid_service == IID_IAccessibleText ||
      guid_service == IID_IAccessibleValue ||
      guid_service == IID_ISimpleDOMDocument ||
      guid_service == IID_ISimpleDOMNode ||
      guid_service == IID_ISimpleDOMText || guid_service == GUID_ISimpleDOM) {
    return QueryInterface(riid, object);
  }

  *object = NULL;
  return E_FAIL;
}

//
// CComObjectRootEx methods.
//

// static
HRESULT WINAPI BrowserAccessibilityComWin::InternalQueryInterface(
    void* this_ptr,
    const _ATL_INTMAP_ENTRY* entries,
    REFIID iid,
    void** object) {
  BrowserAccessibilityComWin* accessibility =
      reinterpret_cast<BrowserAccessibilityComWin*>(this_ptr);

  if (!accessibility->owner()) {
    *object = nullptr;
    return E_NOINTERFACE;
  }

  int32_t ia_role = accessibility->MSAARole();
  if (iid == IID_IAccessibleImage) {
    if (ia_role != ROLE_SYSTEM_GRAPHIC) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTable || iid == IID_IAccessibleTable2) {
    if (ia_role != ROLE_SYSTEM_TABLE) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleTableCell) {
    if (!ui::IsCellOrTableHeaderRole(accessibility->owner()->GetRole())) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleValue) {
    if (!accessibility->IsRangeValueSupported()) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_ISimpleDOMDocument) {
    if (ia_role != ROLE_SYSTEM_DOCUMENT) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  } else if (iid == IID_IAccessibleHyperlink) {
    if (!accessibility || !accessibility->IsHyperlink()) {
      *object = nullptr;
      return E_NOINTERFACE;
    }
  }

  return CComObjectRootBase::InternalQueryInterface(this_ptr, entries, iid,
                                                    object);
}

void BrowserAccessibilityComWin::ComputeStylesIfNeeded() {
  if (!offset_to_text_attributes().empty())
    return;

  std::map<int, std::vector<base::string16>> attributes_map;
  if (owner()->PlatformIsLeaf() || owner()->IsPlainTextField()) {
    attributes_map[0] = ComputeTextAttributes();
    const std::map<int, std::vector<base::string16>> spelling_attributes =
        GetSpellingAttributes();
    MergeSpellingIntoTextAttributes(spelling_attributes, 0 /* start_offset */,
                                    &attributes_map);
    win_attributes_->offset_to_text_attributes.swap(attributes_map);
    return;
  }

  int start_offset = 0;
  for (size_t i = 0; i < owner()->PlatformChildCount(); ++i) {
    auto* child = ToBrowserAccessibilityComWin(owner()->PlatformGetChild(i));
    DCHECK(child);
    std::vector<base::string16> attributes(child->ComputeTextAttributes());

    if (attributes_map.empty()) {
      attributes_map[start_offset] = attributes;
    } else {
      // Only add the attributes for this child if we are at the start of a new
      // style span.
      std::vector<base::string16> previous_attributes =
          attributes_map.rbegin()->second;
      // Must check the size, otherwise if attributes is a subset of
      // prev_attributes, they would appear to be equal.
      if (attributes.size() != previous_attributes.size() ||
          !std::equal(attributes.begin(), attributes.end(),
                      previous_attributes.begin())) {
        attributes_map[start_offset] = attributes;
      }
    }

    if (child->owner()->IsTextOnlyObject()) {
      const std::map<int, std::vector<base::string16>> spelling_attributes =
          child->GetSpellingAttributes();
      MergeSpellingIntoTextAttributes(spelling_attributes, start_offset,
                                      &attributes_map);
      start_offset += child->GetText().length();
    } else {
      start_offset += 1;
    }
  }

  win_attributes_->offset_to_text_attributes.swap(attributes_map);
}

// |offset| could either be a text character or a child index in case of
// non-text objects.
// Currently, to be safe, we convert to text leaf equivalents and we don't use
// tree positions.
// TODO(nektar): Remove this function once selection fixes in Blink are
// thoroughly tested and convert to tree positions.
BrowserAccessibilityPosition::AXPositionInstance
BrowserAccessibilityComWin::CreatePositionForSelectionAt(int offset) const {
  BrowserAccessibilityPositionInstance position =
      owner()->CreatePositionAt(offset)->AsLeafTextPosition();
  if (position->GetAnchor() &&
      position->GetAnchor()->GetRole() == ax::mojom::Role::kInlineTextBox) {
    return position->CreateParentPosition();
    }
    return position;
}

//
// Private methods.
//

void BrowserAccessibilityComWin::UpdateStep1ComputeWinAttributes() {
  // Swap win_attributes_ to old_win_attributes_, allowing us to see
  // exactly what changed and fire appropriate events. Note that
  // old_win_attributes_ is cleared at the end of UpdateStep3FireEvents.
  old_win_attributes_.swap(win_attributes_);

  old_hypertext_ = hypertext_;
  hypertext_ = ui::AXHypertext();

  win_attributes_.reset(new WinAttributes());

  win_attributes_->ia_role = MSAARole();
  win_attributes_->ia_state = MSAAState();
  win_attributes_->role_name = base::UTF8ToUTF16(StringOverrideForMSAARole());

  win_attributes_->ia2_role = ComputeIA2Role();
  // If we didn't explicitly set the IAccessible2 role, make it the same
  // as the MSAA role.
  if (!win_attributes_->ia2_role)
    win_attributes_->ia2_role = win_attributes_->ia_role;

  win_attributes_->ia2_state = ComputeIA2State();
  win_attributes_->ia2_attributes = ComputeIA2Attributes();

  win_attributes_->name =
      owner()->GetString16Attribute(ax::mojom::StringAttribute::kName);

  win_attributes_->description =
      owner()->GetString16Attribute(ax::mojom::StringAttribute::kDescription);

  win_attributes_->value = GetValue();
}

void BrowserAccessibilityComWin::UpdateStep2ComputeHypertext() {
  hypertext_ = ComputeHypertext();
}

void BrowserAccessibilityComWin::UpdateStep3FireEvents(
    bool is_subtree_creation) {
  int32_t state = MSAAState();

  // Fire an event when a new subtree is created.
  if (is_subtree_creation)
    FireNativeEvent(EVENT_OBJECT_SHOW);

  // The rest of the events only fire on changes, not on new objects.

  bool did_fire_namechange = false;

  if (old_win_attributes_->ia_role != 0 ||
      !old_win_attributes_->role_name.empty()) {
    // Fire an event if the name, description, help, or value changes.
    if (name() != old_win_attributes_->name &&
        GetData().GetNameFrom() != ax::mojom::NameFrom::kContents) {
      // Only fire name changes when the name comes from an attribute, otherwise
      // name changes are redundant with text removed/inserted events.
      FireNativeEvent(EVENT_OBJECT_NAMECHANGE);
      did_fire_namechange = true;
    }
    if (description() != old_win_attributes_->description)
      FireNativeEvent(EVENT_OBJECT_DESCRIPTIONCHANGE);
    if (value() != old_win_attributes_->value)
      FireNativeEvent(EVENT_OBJECT_VALUECHANGE);

    // Do not fire EVENT_OBJECT_STATECHANGE if the change was due to a focus
    // change.
    if ((state & ~STATE_SYSTEM_FOCUSED) !=
            (old_win_attributes_->ia_state & ~STATE_SYSTEM_FOCUSED) ||
        ComputeIA2State() != old_win_attributes_->ia2_state) {
      FireNativeEvent(EVENT_OBJECT_STATECHANGE);
    }

    // Handle selection being added or removed.
    bool is_selected_now = (state & STATE_SYSTEM_SELECTED) != 0;
    bool was_selected_before =
        (old_win_attributes_->ia_state & STATE_SYSTEM_SELECTED) != 0;
    if (is_selected_now || was_selected_before) {
      bool multiselect = false;
      if (owner()->PlatformGetParent() &&
          owner()->PlatformGetParent()->HasState(
              ax::mojom::State::kMultiselectable))
        multiselect = true;

      if (multiselect) {
        // In a multi-select box, fire SELECTIONADD and SELECTIONREMOVE events.
        if (is_selected_now && !was_selected_before) {
          FireNativeEvent(EVENT_OBJECT_SELECTIONADD);
        } else if (!is_selected_now && was_selected_before) {
          FireNativeEvent(EVENT_OBJECT_SELECTIONREMOVE);
        }
      } else if (is_selected_now && !was_selected_before) {
        // In a single-select box, only fire SELECTION events.
        FireNativeEvent(EVENT_OBJECT_SELECTION);
      }
    }

    // Fire an event if this container object has scrolled.
    int sx = 0;
    int sy = 0;
    if (owner()->GetIntAttribute(ax::mojom::IntAttribute::kScrollX, &sx) &&
        owner()->GetIntAttribute(ax::mojom::IntAttribute::kScrollY, &sy)) {
      if (sx != previous_scroll_x_ || sy != previous_scroll_y_)
        FireNativeEvent(EVENT_SYSTEM_SCROLLINGEND);
      previous_scroll_x_ = sx;
      previous_scroll_y_ = sy;
    }

    // Fire hypertext-related events.
    // Do not fire removed/inserted when a name change event was also fired, as
    // they are providing redundant information and will lead to duplicate
    // announcements.
    if (!did_fire_namechange) {
      int start, old_len, new_len;
      ComputeHypertextRemovedAndInserted(&start, &old_len, &new_len);
      if (old_len > 0) {
        // In-process screen readers may call IAccessibleText::get_oldText
        // in reaction to this event to retrieve the text that was removed.
        FireNativeEvent(IA2_EVENT_TEXT_REMOVED);
      }
      if (new_len > 0) {
        // In-process screen readers may call IAccessibleText::get_newText
        // in reaction to this event to retrieve the text that was inserted.
        FireNativeEvent(IA2_EVENT_TEXT_INSERTED);
      }
    }

    // Changing a static text node can affect the IA2 hypertext of its parent
    // and, if the node is in a simple text control, the hypertext of the text
    // control itself.
    BrowserAccessibilityComWin* parent =
        ToBrowserAccessibilityComWin(owner()->PlatformGetParent());
    if (parent && (parent->owner()->HasState(ax::mojom::State::kEditable) ||
                   owner()->IsTextOnlyObject())) {
      parent->owner()->UpdatePlatformAttributes();
    }
  }

  old_win_attributes_.reset(nullptr);
  old_hypertext_ = ui::AXHypertext();
}

BrowserAccessibilityManager* BrowserAccessibilityComWin::Manager() const {
  DCHECK(owner());

  auto* manager = owner()->manager();
  DCHECK(manager);
  return manager;
}

//
// AXPlatformNode overrides
//
void BrowserAccessibilityComWin::Destroy() {
  // Detach BrowserAccessibilityWin from us.
  owner_ = nullptr;
  AXPlatformNodeWin::Destroy();
}

void BrowserAccessibilityComWin::Init(ui::AXPlatformNodeDelegate* delegate) {
  owner_ = static_cast<BrowserAccessibilityWin*>(delegate);
  AXPlatformNodeWin::Init(delegate);
}

base::string16 BrowserAccessibilityComWin::GetInvalidValue() const {
  const BrowserAccessibilityWin* target = owner();
  // The aria-invalid=spelling/grammar need to be exposed as text attributes for
  // a range matching the visual underline representing the error.
  if (static_cast<ax::mojom::InvalidState>(
          target->GetIntAttribute(ax::mojom::IntAttribute::kInvalidState)) ==
          ax::mojom::InvalidState::kNone &&
      target->IsTextOnlyObject() && target->PlatformGetParent()) {
    // Text nodes need to reflect the invalid state of their parent object,
    // otherwise spelling and grammar errors communicated through aria-invalid
    // won't be reflected in text attributes.
    target = static_cast<BrowserAccessibilityWin*>(target->PlatformGetParent());
  }

  base::string16 invalid_value;
  // Note: spelling+grammar errors case is disallowed and not supported. It
  // could possibly arise with aria-invalid on the ancestor of a spelling error,
  // but this is not currently described in any spec and no real-world use cases
  // have been found.
  switch (static_cast<ax::mojom::InvalidState>(
      target->GetIntAttribute(ax::mojom::IntAttribute::kInvalidState))) {
    case ax::mojom::InvalidState::kNone:
    case ax::mojom::InvalidState::kFalse:
      break;
    case ax::mojom::InvalidState::kTrue:
      return invalid_value = L"true";
    case ax::mojom::InvalidState::kSpelling:
      return invalid_value = L"spelling";
    case ax::mojom::InvalidState::kGrammar:
      return base::ASCIIToUTF16("grammar");
    case ax::mojom::InvalidState::kOther: {
      base::string16 aria_invalid_value;
      if (target->GetString16Attribute(
              ax::mojom::StringAttribute::kAriaInvalidValue,
              &aria_invalid_value)) {
        SanitizeStringAttributeForIA2(aria_invalid_value, &aria_invalid_value);
        invalid_value = aria_invalid_value;
      } else {
        // Set the attribute to L"true", since we cannot be more specific.
        invalid_value = L"true";
      }
    }
  }
  return invalid_value;
}

std::vector<base::string16> BrowserAccessibilityComWin::ComputeTextAttributes()
    const {
  std::vector<base::string16> attributes;

  // We include list markers for now, but there might be other objects that are
  // auto generated.
  // TODO(nektar): Compute what objects are auto-generated in Blink.
  if (owner()->GetRole() == ax::mojom::Role::kListMarker)
    attributes.push_back(L"auto-generated:true");

  int color;
  if (owner()->GetIntAttribute(ax::mojom::IntAttribute::kBackgroundColor,
                               &color)) {
    unsigned int alpha = SkColorGetA(color);
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    // Don't expose default value of pure white.
    if (alpha && (red != 255 || green != 255 || blue != 255)) {
      base::string16 color_value = L"rgb(" + base::UintToString16(red) + L',' +
                                   base::UintToString16(green) + L',' +
                                   base::UintToString16(blue) + L')';
      SanitizeStringAttributeForIA2(color_value, &color_value);
      attributes.push_back(L"background-color:" + color_value);
    }
  }

  if (owner()->GetIntAttribute(ax::mojom::IntAttribute::kColor, &color)) {
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    // Don't expose default value of black.
    if (red || green || blue) {
      base::string16 color_value = L"rgb(" + base::UintToString16(red) + L',' +
                                   base::UintToString16(green) + L',' +
                                   base::UintToString16(blue) + L')';
      SanitizeStringAttributeForIA2(color_value, &color_value);
      attributes.push_back(L"color:" + color_value);
    }
  }

  base::string16 font_family(owner()->GetInheritedString16Attribute(
      ax::mojom::StringAttribute::kFontFamily));
  // Attribute has no default value.
  if (!font_family.empty()) {
    SanitizeStringAttributeForIA2(font_family, &font_family);
    attributes.push_back(L"font-family:" + font_family);
  }

  float font_size;
  // Attribute has no default value.
  if (GetFloatAttribute(ax::mojom::FloatAttribute::kFontSize, &font_size)) {
    // The IA2 Spec requires the value to be in pt, not in pixels.
    // There are 72 points per inch.
    // We assume that there are 96 pixels per inch on a standard display.
    // TODO(nektar): Figure out the current value of pixels per inch.
    float points = font_size * 72.0 / 96.0;
    attributes.push_back(L"font-size:" + base::NumberToString16(points) +
                         L"pt");
  }

  // TODO(nektar): Add Blink support for the following attributes:
  // text-line-through-mode, text-line-through-width, text-outline:false,
  // text-position:baseline, text-shadow:none, text-underline-mode:continuous.

  int32_t text_style =
      owner()->GetIntAttribute(ax::mojom::IntAttribute::kTextStyle);
  if (text_style != static_cast<int32_t>(ax::mojom::TextStyle::kNone)) {
    if (text_style &
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleItalic)) {
      attributes.push_back(L"font-style:italic");
    }

    if (text_style &
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleBold)) {
      attributes.push_back(L"font-weight:bold");
    }

    if (text_style &
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleLineThrough)) {
      // TODO(nektar): Figure out a more specific value.
      attributes.push_back(L"text-line-through-style:solid");
    }

    if (text_style &
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleUnderline)) {
      // TODO(nektar): Figure out a more specific value.
      attributes.push_back(L"text-underline-style:solid");
    }
  }

  // Screen readers look at the text attributes to determine if something is
  // misspelled, so we need to propagate any spelling attributes from immediate
  // parents of text-only objects.
  base::string16 invalid_value = GetInvalidValue();
  if (!invalid_value.empty())
    attributes.push_back(L"invalid:" + invalid_value);

  base::string16 language(owner()->GetInheritedString16Attribute(
      ax::mojom::StringAttribute::kLanguage));
  // Don't expose default value should of L"en-US".
  if (!language.empty() && language != L"en-US") {
    SanitizeStringAttributeForIA2(language, &language);
    attributes.push_back(L"language:" + language);
  }

  auto text_direction = static_cast<ax::mojom::TextDirection>(
      owner()->GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));
  switch (text_direction) {
    case ax::mojom::TextDirection::kNone:
    case ax::mojom::TextDirection::kLtr:
      break;
    case ax::mojom::TextDirection::kRtl:
      attributes.push_back(L"writing-mode:rl");
      break;
    case ax::mojom::TextDirection::kTtb:
      attributes.push_back(L"writing-mode:tb");
      break;
    case ax::mojom::TextDirection::kBtt:
      // Not listed in the IA2 Spec.
      attributes.push_back(L"writing-mode:bt");
      break;
  }

  auto text_position = static_cast<ax::mojom::TextPosition>(
      owner()->GetIntAttribute(ax::mojom::IntAttribute::kTextPosition));
  switch (text_position) {
    case ax::mojom::TextPosition::kNone:
      break;
    case ax::mojom::TextPosition::kSubscript:
      attributes.push_back(L"text-position:sub");
      break;
    case ax::mojom::TextPosition::kSuperscript:
      attributes.push_back(L"text-position:super");
      break;
  }

  return attributes;
}

BrowserAccessibilityComWin* BrowserAccessibilityComWin::NewReference() {
  AddRef();
  return this;
}

std::map<int, std::vector<base::string16>>
BrowserAccessibilityComWin::GetSpellingAttributes() {
  std::map<int, std::vector<base::string16>> spelling_attributes;
  if (owner()->IsTextOnlyObject()) {
    const std::vector<int32_t>& marker_types =
        owner()->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int>& marker_starts = owner()->GetIntListAttribute(
        ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& marker_ends =
        owner()->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
    for (size_t i = 0; i < marker_types.size(); ++i) {
      if (!(marker_types[i] &
            static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)))
        continue;
      int start_offset = marker_starts[i];
      int end_offset = marker_ends[i];
      std::vector<base::string16> start_attributes;
      start_attributes.push_back(L"invalid:spelling");
      spelling_attributes[start_offset] = start_attributes;
      spelling_attributes[end_offset] = std::vector<base::string16>();
    }
  }
  if (owner()->IsPlainTextField()) {
    int start_offset = 0;
    for (BrowserAccessibility* static_text =
             BrowserAccessibilityManager::NextTextOnlyObject(
                 owner()->InternalGetChild(0));
         static_text; static_text = static_text->GetNextSibling()) {
      auto* text_win = ToBrowserAccessibilityComWin(static_text);
      if (text_win) {
        std::map<int, std::vector<base::string16>> text_spelling_attributes =
            text_win->GetSpellingAttributes();
        for (auto& attribute : text_spelling_attributes) {
          spelling_attributes[start_offset + attribute.first] =
              std::move(attribute.second);
        }
        start_offset += static_cast<int>(text_win->GetText().length());
      }
    }
  }
  return spelling_attributes;
}

BrowserAccessibilityComWin* BrowserAccessibilityComWin::GetTargetFromChildID(
    const VARIANT& var_id) {
  if (!owner())
    return nullptr;

  if (var_id.vt != VT_I4)
    return nullptr;

  LONG child_id = var_id.lVal;
  if (child_id == CHILDID_SELF)
    return this;

  if (child_id >= 1 &&
      child_id <= static_cast<LONG>(owner()->PlatformChildCount()))
    return ToBrowserAccessibilityComWin(
        owner()->PlatformGetChild(child_id - 1));

  auto* child = static_cast<BrowserAccessibilityComWin*>(
      AXPlatformNodeWin::GetFromUniqueId(-child_id));
  if (child && child->owner()->IsDescendantOf(owner()))
    return child;

  return nullptr;
}

HRESULT BrowserAccessibilityComWin::GetStringAttributeAsBstr(
    ax::mojom::StringAttribute attribute,
    BSTR* value_bstr) {
  base::string16 str;
  if (!owner())
    return E_FAIL;

  if (!owner()->GetString16Attribute(attribute, &str))
    return S_FALSE;

  *value_bstr = SysAllocString(str.c_str());
  DCHECK(*value_bstr);

  return S_OK;
}

// Pass in prefix with ":" included at the end, e.g. "invalid:".
bool HasAttribute(std::vector<base::string16>& existing_attributes,
                  base::string16 prefix) {
  for (base::string16& attr : existing_attributes) {
    if (base::StartsWith(attr, prefix, base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

// static
void BrowserAccessibilityComWin::MergeSpellingIntoTextAttributes(
    const std::map<int, std::vector<base::string16>>& spelling_attributes,
    int start_offset,
    std::map<int, std::vector<base::string16>>* text_attributes) {
  if (!text_attributes) {
    NOTREACHED();
    return;
  }

  for (const auto& spelling_attribute : spelling_attributes) {
    int offset = start_offset + spelling_attribute.first;
    const auto iterator = text_attributes->find(offset);
    if (iterator == text_attributes->end()) {
      text_attributes->emplace(offset, spelling_attribute.second);
    } else {
      std::vector<base::string16>& existing_attributes = iterator->second;
      // There might be a spelling attribute already in the list of text
      // attributes, originating from "aria-invalid", that is being overwritten
      // by a spelling marker. If it already exists, prefer it over this
      // automatically computed attribute.
      if (!HasAttribute(existing_attributes, L"invalid:")) {
        // Does not exist -- insert our own.
        existing_attributes.insert(existing_attributes.end(),
                                   spelling_attribute.second.begin(),
                                   spelling_attribute.second.end());
      }
    }
  }
}

// Static
void BrowserAccessibilityComWin::SanitizeStringAttributeForIA2(
    const base::string16& input,
    base::string16* output) {
  DCHECK(output);
  // According to the IA2 Spec, these characters need to be escaped with a
  // backslash: backslash, colon, comma, equals and semicolon.
  // Note that backslash must be replaced first.
  base::ReplaceChars(input, L"\\", L"\\\\", output);
  base::ReplaceChars(*output, L":", L"\\:", output);
  base::ReplaceChars(*output, L",", L"\\,", output);
  base::ReplaceChars(*output, L"=", L"\\=", output);
  base::ReplaceChars(*output, L";", L"\\;", output);
}

void BrowserAccessibilityComWin::SetIA2HypertextSelection(LONG start_offset,
                                                          LONG end_offset) {
  HandleSpecialTextOffset(&start_offset);
  HandleSpecialTextOffset(&end_offset);
  BrowserAccessibilityPositionInstance start_position =
      CreatePositionForSelectionAt(static_cast<int>(start_offset));
  BrowserAccessibilityPositionInstance end_position =
      CreatePositionForSelectionAt(static_cast<int>(end_offset));
  Manager()->SetSelection(
      AXPlatformRange(std::move(start_position), std::move(end_position)));
}

LONG BrowserAccessibilityComWin::FindBoundary(
    IA2TextBoundaryType ia2_boundary,
    LONG start_offset,
    ui::TextBoundaryDirection direction) {
  HandleSpecialTextOffset(&start_offset);
  // If the |start_offset| is equal to the location of the caret, then use the
  // focus affinity, otherwise default to downstream affinity.
  ax::mojom::TextAffinity affinity = ax::mojom::TextAffinity::kDownstream;
  int selection_start, selection_end;
  GetSelectionOffsets(&selection_start, &selection_end);
  if (selection_end >= 0 && start_offset == selection_end)
    affinity = Manager()->GetTreeData().sel_focus_affinity;

  if (ia2_boundary == IA2_TEXT_BOUNDARY_WORD) {
    switch (direction) {
      case ui::FORWARDS_DIRECTION: {
        BrowserAccessibilityPositionInstance position =
            owner()->CreatePositionAt(static_cast<int>(start_offset), affinity);
        BrowserAccessibilityPositionInstance next_word =
            position->CreateNextWordStartPosition(
                ui::AXBoundaryBehavior::StopAtAnchorBoundary);
        return next_word->text_offset();
      }
      case ui::BACKWARDS_DIRECTION: {
        BrowserAccessibilityPositionInstance position =
            owner()->CreatePositionAt(static_cast<int>(start_offset), affinity);
        BrowserAccessibilityPositionInstance previous_word =
            position->CreatePreviousWordStartPosition(
                ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
        return previous_word->text_offset();
      }
    }
  }

  if (ia2_boundary == IA2_TEXT_BOUNDARY_LINE) {
    switch (direction) {
      case ui::FORWARDS_DIRECTION: {
        BrowserAccessibilityPositionInstance position =
            owner()->CreatePositionAt(static_cast<int>(start_offset), affinity);
        BrowserAccessibilityPositionInstance next_line =
            position->CreateNextLineStartPosition(
                ui::AXBoundaryBehavior::StopAtAnchorBoundary);
        return next_line->text_offset();
      }
      case ui::BACKWARDS_DIRECTION: {
        BrowserAccessibilityPositionInstance position =
            owner()->CreatePositionAt(static_cast<int>(start_offset), affinity);
        BrowserAccessibilityPositionInstance previous_line =
            position->CreatePreviousLineStartPosition(
                ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
        return previous_line->text_offset();
      }
    }
  }

  // TODO(nektar): |AXPosition| can handle other types of boundaries as well.
  ui::TextBoundaryType boundary = IA2TextBoundaryToTextBoundary(ia2_boundary);
  return ui::FindAccessibleTextBoundary(
      GetText(), owner()->GetLineStartOffsets(), boundary, start_offset,
      direction, affinity);
}

LONG BrowserAccessibilityComWin::FindStartOfStyle(
    LONG start_offset,
    ui::TextBoundaryDirection direction) {
  LONG text_length = static_cast<LONG>(GetText().length());
  DCHECK_GE(start_offset, 0);
  DCHECK_LE(start_offset, text_length);

  switch (direction) {
    case ui::BACKWARDS_DIRECTION: {
      if (offset_to_text_attributes().empty())
        return 0;

      auto iterator = offset_to_text_attributes().upper_bound(start_offset);
      --iterator;
      return static_cast<LONG>(iterator->first);
    }
    case ui::FORWARDS_DIRECTION: {
      const auto iterator =
          offset_to_text_attributes().upper_bound(start_offset);
      if (iterator == offset_to_text_attributes().end())
        return text_length;
      return static_cast<LONG>(iterator->first);
    }
  }

  NOTREACHED();
  return start_offset;
}

BrowserAccessibilityComWin* BrowserAccessibilityComWin::GetFromID(
    int32_t id) const {
  if (!owner())
    return nullptr;
  return ToBrowserAccessibilityComWin(Manager()->GetFromID(id));
}

bool BrowserAccessibilityComWin::IsListBoxOptionOrMenuListOption() {
  if (!owner()->PlatformGetParent())
    return false;

  ax::mojom::Role role = owner()->GetRole();
  ax::mojom::Role parent_role = owner()->PlatformGetParent()->GetRole();

  if (role == ax::mojom::Role::kListBoxOption &&
      parent_role == ax::mojom::Role::kListBox) {
    return true;
  }

  if (role == ax::mojom::Role::kMenuListOption &&
      parent_role == ax::mojom::Role::kMenuListPopup) {
    return true;
  }

  return false;
}

void BrowserAccessibilityComWin::FireNativeEvent(LONG win_event_type) const {
  if (owner()->PlatformIsChildOfLeaf())
    return;
  Manager()->ToBrowserAccessibilityManagerWin()->FireWinAccessibilityEvent(
      win_event_type, owner());
}

BrowserAccessibilityComWin* ToBrowserAccessibilityComWin(
    BrowserAccessibility* obj) {
  if (!obj || !obj->IsNative())
    return nullptr;
  auto* result = static_cast<BrowserAccessibilityWin*>(obj)->GetCOM();
  return result;
}

}  // namespace content
