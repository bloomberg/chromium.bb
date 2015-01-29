// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_android.h"

#include "base/i18n/break_iterator.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/common/accessibility_messages.h"

namespace {

// These are enums from android.text.InputType in Java:
enum {
  ANDROID_TEXT_INPUTTYPE_TYPE_NULL = 0,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME = 0x4,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE = 0x14,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME = 0x24,
  ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER = 0x2,
  ANDROID_TEXT_INPUTTYPE_TYPE_PHONE = 0x3,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT = 0x1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI = 0x11,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EDIT_TEXT = 0xa1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL = 0xd1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD = 0xe1
};

// These are enums from android.view.View in Java:
enum {
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE = 0,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE = 1,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE = 2
};

// These are enums from
// android.view.accessibility.AccessibilityNodeInfo.RangeInfo in Java:
enum {
  ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT = 1
};

}  // namespace

namespace content {

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityAndroid();
}

BrowserAccessibilityAndroid::BrowserAccessibilityAndroid() {
  first_time_ = true;
}

bool BrowserAccessibilityAndroid::IsNative() const {
  return true;
}

void BrowserAccessibilityAndroid::OnLocationChanged() {
  manager()->NotifyAccessibilityEvent(ui::AX_EVENT_LOCATION_CHANGED, this);
}

bool BrowserAccessibilityAndroid::PlatformIsLeaf() const {
  if (InternalChildCount() == 0)
    return true;

  // Iframes are always allowed to contain children.
  if (IsIframe() ||
      GetRole() == ui::AX_ROLE_ROOT_WEB_AREA ||
      GetRole() == ui::AX_ROLE_WEB_AREA) {
    return false;
  }

  // If it has a focusable child, we definitely can't leave out children.
  if (HasFocusableChild())
    return false;

  // Date and time controls should drop their children.
  if (GetRole() == ui::AX_ROLE_DATE || GetRole() == ui::AX_ROLE_TIME)
    return true;

  // Headings with text can drop their children.
  base::string16 name = GetText();
  if (GetRole() == ui::AX_ROLE_HEADING && !name.empty())
    return true;

  // Focusable nodes with text can drop their children.
  if (HasState(ui::AX_STATE_FOCUSABLE) && !name.empty())
    return true;

  // Nodes with only static text as children can drop their children.
  if (HasOnlyStaticTextChildren())
    return true;

  return BrowserAccessibility::PlatformIsLeaf();
}

bool BrowserAccessibilityAndroid::CanScrollForward() const {
  if (!IsSlider())
    return false;

  float value = GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE);
  float max = GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE);
  return value < max;
}

bool BrowserAccessibilityAndroid::CanScrollBackward() const {
  if (!IsSlider())
    return false;

  float value = GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE);
  float min = GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE);
  return value > min;
}

bool BrowserAccessibilityAndroid::IsCheckable() const {
  bool checkable = false;
  bool is_aria_pressed_defined;
  bool is_mixed;
  GetAriaTristate("aria-pressed", &is_aria_pressed_defined, &is_mixed);
  if (GetRole() == ui::AX_ROLE_CHECK_BOX ||
      GetRole() == ui::AX_ROLE_RADIO_BUTTON ||
      GetRole() == ui::AX_ROLE_MENU_ITEM_CHECK_BOX ||
      GetRole() == ui::AX_ROLE_MENU_ITEM_RADIO ||
      is_aria_pressed_defined) {
    checkable = true;
  }
  if (HasState(ui::AX_STATE_CHECKED))
    checkable = true;
  return checkable;
}

bool BrowserAccessibilityAndroid::IsChecked() const {
  return (HasState(ui::AX_STATE_CHECKED) || HasState(ui::AX_STATE_PRESSED));
}

bool BrowserAccessibilityAndroid::IsClickable() const {
  if (!PlatformIsLeaf())
    return false;

  return IsFocusable() || !GetText().empty();
}

bool BrowserAccessibilityAndroid::IsCollection() const {
  return (GetRole() == ui::AX_ROLE_GRID ||
          GetRole() == ui::AX_ROLE_LIST ||
          GetRole() == ui::AX_ROLE_LIST_BOX ||
          GetRole() == ui::AX_ROLE_DESCRIPTION_LIST ||
          GetRole() == ui::AX_ROLE_TABLE ||
          GetRole() == ui::AX_ROLE_TREE);
}

bool BrowserAccessibilityAndroid::IsCollectionItem() const {
  return (GetRole() == ui::AX_ROLE_CELL ||
          GetRole() == ui::AX_ROLE_COLUMN_HEADER ||
          GetRole() == ui::AX_ROLE_DESCRIPTION_LIST_TERM ||
          GetRole() == ui::AX_ROLE_LIST_BOX_OPTION ||
          GetRole() == ui::AX_ROLE_LIST_ITEM ||
          GetRole() == ui::AX_ROLE_ROW_HEADER ||
          GetRole() == ui::AX_ROLE_TREE_ITEM);
}

bool BrowserAccessibilityAndroid::IsContentInvalid() const {
  std::string invalid;
  return GetHtmlAttribute("aria-invalid", &invalid);
}

bool BrowserAccessibilityAndroid::IsDismissable() const {
  return false;  // No concept of "dismissable" on the web currently.
}

bool BrowserAccessibilityAndroid::IsEditableText() const {
  return (GetRole() == ui::AX_ROLE_TEXT_AREA ||
          GetRole() == ui::AX_ROLE_TEXT_FIELD);
}

bool BrowserAccessibilityAndroid::IsEnabled() const {
  return HasState(ui::AX_STATE_ENABLED);
}

bool BrowserAccessibilityAndroid::IsFocusable() const {
  bool focusable = HasState(ui::AX_STATE_FOCUSABLE);
  if (IsIframe() ||
      GetRole() == ui::AX_ROLE_WEB_AREA) {
    focusable = false;
  }
  return focusable;
}

bool BrowserAccessibilityAndroid::IsFocused() const {
  return manager()->GetFocus(manager()->GetRoot()) == this;
}

bool BrowserAccessibilityAndroid::IsHeading() const {
  BrowserAccessibilityAndroid* parent =
      static_cast<BrowserAccessibilityAndroid*>(GetParent());
  if (parent && parent->IsHeading())
    return true;

  return (GetRole() == ui::AX_ROLE_COLUMN_HEADER ||
          GetRole() == ui::AX_ROLE_HEADING ||
          GetRole() == ui::AX_ROLE_ROW_HEADER);
}

bool BrowserAccessibilityAndroid::IsHierarchical() const {
  return (GetRole() == ui::AX_ROLE_LIST ||
          GetRole() == ui::AX_ROLE_DESCRIPTION_LIST ||
          GetRole() == ui::AX_ROLE_TREE);
}

bool BrowserAccessibilityAndroid::IsLink() const {
  return (GetRole() == ui::AX_ROLE_LINK ||
         GetRole() == ui::AX_ROLE_IMAGE_MAP_LINK);
}

bool BrowserAccessibilityAndroid::IsMultiLine() const {
  return GetRole() == ui::AX_ROLE_TEXT_AREA;
}

bool BrowserAccessibilityAndroid::IsPassword() const {
  return HasState(ui::AX_STATE_PROTECTED);
}

bool BrowserAccessibilityAndroid::IsRangeType() const {
  return (GetRole() == ui::AX_ROLE_PROGRESS_INDICATOR ||
          GetRole() == ui::AX_ROLE_METER ||
          GetRole() == ui::AX_ROLE_SCROLL_BAR ||
          GetRole() == ui::AX_ROLE_SLIDER);
}

bool BrowserAccessibilityAndroid::IsScrollable() const {
  int dummy;
  return GetIntAttribute(ui::AX_ATTR_SCROLL_X_MAX, &dummy);
}

bool BrowserAccessibilityAndroid::IsSelected() const {
  return HasState(ui::AX_STATE_SELECTED);
}

bool BrowserAccessibilityAndroid::IsSlider() const {
  return GetRole() == ui::AX_ROLE_SLIDER;
}

bool BrowserAccessibilityAndroid::IsVisibleToUser() const {
  return !HasState(ui::AX_STATE_INVISIBLE);
}

bool BrowserAccessibilityAndroid::CanOpenPopup() const {
  return HasState(ui::AX_STATE_HASPOPUP);
}

const char* BrowserAccessibilityAndroid::GetClassName() const {
  const char* class_name = NULL;

  switch (GetRole()) {
    case ui::AX_ROLE_SPIN_BUTTON:
    case ui::AX_ROLE_TEXT_AREA:
    case ui::AX_ROLE_TEXT_FIELD:
      class_name = "android.widget.EditText";
      break;
    case ui::AX_ROLE_SLIDER:
      class_name = "android.widget.SeekBar";
      break;
    case ui::AX_ROLE_COLOR_WELL:
    case ui::AX_ROLE_COMBO_BOX:
    case ui::AX_ROLE_DATE:
    case ui::AX_ROLE_POP_UP_BUTTON:
    case ui::AX_ROLE_TIME:
      class_name = "android.widget.Spinner";
      break;
    case ui::AX_ROLE_BUTTON:
    case ui::AX_ROLE_MENU_BUTTON:
      class_name = "android.widget.Button";
      break;
    case ui::AX_ROLE_CHECK_BOX:
      class_name = "android.widget.CheckBox";
      break;
    case ui::AX_ROLE_RADIO_BUTTON:
      class_name = "android.widget.RadioButton";
      break;
    case ui::AX_ROLE_TOGGLE_BUTTON:
      class_name = "android.widget.ToggleButton";
      break;
    case ui::AX_ROLE_CANVAS:
    case ui::AX_ROLE_IMAGE:
    case ui::AX_ROLE_SVG_ROOT:
      class_name = "android.widget.Image";
      break;
    case ui::AX_ROLE_METER:
    case ui::AX_ROLE_PROGRESS_INDICATOR:
      class_name = "android.widget.ProgressBar";
      break;
    case ui::AX_ROLE_TAB_LIST:
      class_name = "android.widget.TabWidget";
      break;
    case ui::AX_ROLE_GRID:
    case ui::AX_ROLE_TABLE:
      class_name = "android.widget.GridView";
      break;
    case ui::AX_ROLE_LIST:
    case ui::AX_ROLE_LIST_BOX:
    case ui::AX_ROLE_DESCRIPTION_LIST:
      class_name = "android.widget.ListView";
      break;
    case ui::AX_ROLE_DIALOG:
      class_name = "android.app.Dialog";
      break;
    case ui::AX_ROLE_ROOT_WEB_AREA:
      class_name = "android.webkit.WebView";
      break;
    case ui::AX_ROLE_MENU_ITEM:
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
    case ui::AX_ROLE_MENU_ITEM_RADIO:
      class_name = "android.view.MenuItem";
      break;
    default:
      class_name = "android.view.View";
      break;
  }

  return class_name;
}

base::string16 BrowserAccessibilityAndroid::GetText() const {
  if (IsIframe() ||
      GetRole() == ui::AX_ROLE_WEB_AREA) {
    return base::string16();
  }

  // See comment in browser_accessibility_win.cc for details.
  // The difference here is that we can only expose one accessible
  // name on Android, not 2 or 3 like on Windows or Mac.

  // First, always return the |value| attribute if this is an
  // input field.
  base::string16 value = GetString16Attribute(ui::AX_ATTR_VALUE);
  if (!value.empty()) {
    if (HasState(ui::AX_STATE_EDITABLE))
      return value;

    switch (GetRole()) {
      case ui::AX_ROLE_COMBO_BOX:
      case ui::AX_ROLE_POP_UP_BUTTON:
      case ui::AX_ROLE_TEXT_AREA:
      case ui::AX_ROLE_TEXT_FIELD:
        return value;
    }
  }

  // For color wells, the color is stored in separate attributes.
  // Perhaps we could return color names in the future?
  if (GetRole() == ui::AX_ROLE_COLOR_WELL) {
    int red = GetIntAttribute(ui::AX_ATTR_COLOR_VALUE_RED);
    int green = GetIntAttribute(ui::AX_ATTR_COLOR_VALUE_GREEN);
    int blue = GetIntAttribute(ui::AX_ATTR_COLOR_VALUE_BLUE);
    return base::UTF8ToUTF16(
        base::StringPrintf("#%02X%02X%02X", red, green, blue));
  }

  // Always prefer visible text if this is a link. Sites sometimes add
  // a "title" attribute to a link with more information, but we can't
  // lose the link text.
  base::string16 name = GetString16Attribute(ui::AX_ATTR_NAME);
  if (!name.empty() && GetRole() == ui::AX_ROLE_LINK)
    return name;

  // If there's no text value, the basic rule is: prefer description
  // (aria-labelledby or aria-label), then help (title), then name
  // (inner text), then value (control value).  However, if
  // title_elem_id is set, that means there's a label element
  // supplying the name and then name takes precedence over help.
  // TODO(dmazzoni): clean this up by providing more granular labels in
  // Blink, making the platform-specific mapping to accessible text simpler.
  base::string16 description = GetString16Attribute(ui::AX_ATTR_DESCRIPTION);
  base::string16 help = GetString16Attribute(ui::AX_ATTR_HELP);

  base::string16 placeholder;
  switch (GetRole()) {
    case ui::AX_ROLE_DATE:
    case ui::AX_ROLE_TEXT_AREA:
    case ui::AX_ROLE_TEXT_FIELD:
    case ui::AX_ROLE_TIME:
      GetHtmlAttribute("placeholder", &placeholder);
  }

  int title_elem_id = GetIntAttribute(
      ui::AX_ATTR_TITLE_UI_ELEMENT);
  base::string16 text;
  if (!description.empty())
    text = description;
  else if (title_elem_id && !name.empty())
    text = name;
  else if (!help.empty())
    text = help;
  else if (!name.empty())
    text = name;
  else if (!placeholder.empty())
    text = placeholder;
  else if (!value.empty())
    text = value;
  else if (title_elem_id) {
    BrowserAccessibility* title_elem =
          manager()->GetFromID(title_elem_id);
    if (title_elem)
      text = static_cast<BrowserAccessibilityAndroid*>(title_elem)->GetText();
  }

  // This is called from PlatformIsLeaf, so don't call PlatformChildCount
  // from within this!
  if (text.empty() &&
      (HasOnlyStaticTextChildren() ||
       (IsFocusable() && HasOnlyTextAndImageChildren()))) {
    for (uint32 i = 0; i < InternalChildCount(); i++) {
      BrowserAccessibility* child = InternalGetChild(i);
      text += static_cast<BrowserAccessibilityAndroid*>(child)->GetText();
    }
  }

  if (text.empty() && (IsLink() || GetRole() == ui::AX_ROLE_IMAGE)) {
    base::string16 url = GetString16Attribute(ui::AX_ATTR_URL);
    // Given a url like http://foo.com/bar/baz.png, just return the
    // base name, e.g., "baz".
    int trailing_slashes = 0;
    while (url.size() - trailing_slashes > 0 &&
           url[url.size() - trailing_slashes - 1] == '/') {
      trailing_slashes++;
    }
    if (trailing_slashes)
      url = url.substr(0, url.size() - trailing_slashes);
    size_t slash_index = url.rfind('/');
    if (slash_index != std::string::npos)
      url = url.substr(slash_index + 1);
    size_t dot_index = url.rfind('.');
    if (dot_index != std::string::npos)
      url = url.substr(0, dot_index);
    text = url;
  }

  return text;
}

int BrowserAccessibilityAndroid::GetItemIndex() const {
  int index = 0;
  switch (GetRole()) {
    case ui::AX_ROLE_LIST_ITEM:
    case ui::AX_ROLE_LIST_BOX_OPTION:
    case ui::AX_ROLE_TREE_ITEM:
      index = GetIndexInParent();
      break;
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_PROGRESS_INDICATOR: {
      // Return a percentage here for live feedback in an AccessibilityEvent.
      // The exact value is returned in RangeCurrentValue.
      float min = GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE);
      float max = GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE);
      float value = GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE);
      if (max > min && value >= min && value <= max)
        index = static_cast<int>(((value - min)) * 100 / (max - min));
      break;
    }
  }
  return index;
}

int BrowserAccessibilityAndroid::GetItemCount() const {
  int count = 0;
  switch (GetRole()) {
    case ui::AX_ROLE_LIST:
    case ui::AX_ROLE_LIST_BOX:
    case ui::AX_ROLE_DESCRIPTION_LIST:
      count = PlatformChildCount();
      break;
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_PROGRESS_INDICATOR:
      // An AccessibilityEvent can only return integer information about a
      // seek control, so we return a percentage. The real range is returned
      // in RangeMin and RangeMax.
      count = 100;
      break;
  }
  return count;
}

int BrowserAccessibilityAndroid::GetScrollX() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_X, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetScrollY() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_Y, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMaxScrollX() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_X_MAX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMaxScrollY() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_Y_MAX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetTextChangeFromIndex() const {
  size_t index = 0;
  while (index < old_value_.length() &&
         index < new_value_.length() &&
         old_value_[index] == new_value_[index]) {
    index++;
  }
  return index;
}

int BrowserAccessibilityAndroid::GetTextChangeAddedCount() const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (new_len - left - right);
}

int BrowserAccessibilityAndroid::GetTextChangeRemovedCount() const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (old_len - left - right);
}

base::string16 BrowserAccessibilityAndroid::GetTextChangeBeforeText() const {
  return old_value_;
}

int BrowserAccessibilityAndroid::GetSelectionStart() const {
  int sel_start = 0;
  GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START, &sel_start);
  return sel_start;
}

int BrowserAccessibilityAndroid::GetSelectionEnd() const {
  int sel_end = 0;
  GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END, &sel_end);
  return sel_end;
}

int BrowserAccessibilityAndroid::GetEditableTextLength() const {
  base::string16 value = GetString16Attribute(ui::AX_ATTR_VALUE);
  return value.length();
}

int BrowserAccessibilityAndroid::AndroidInputType() const {
  std::string html_tag = GetStringAttribute(
      ui::AX_ATTR_HTML_TAG);
  if (html_tag != "input")
    return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;

  std::string type;
  if (!GetHtmlAttribute("type", &type))
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;

  if (type.empty() || type == "text" || type == "search")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;
  else if (type == "date")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  else if (type == "datetime" || type == "datetime-local")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;
  else if (type == "email")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL;
  else if (type == "month")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  else if (type == "number")
    return ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER;
  else if (type == "password")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD;
  else if (type == "tel")
    return ANDROID_TEXT_INPUTTYPE_TYPE_PHONE;
  else if (type == "time")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME;
  else if (type == "url")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI;
  else if (type == "week")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;

  return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;
}

int BrowserAccessibilityAndroid::AndroidLiveRegionType() const {
  std::string live = GetStringAttribute(
      ui::AX_ATTR_LIVE_STATUS);
  if (live == "polite")
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE;
  else if (live == "assertive")
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE;
  return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE;
}

int BrowserAccessibilityAndroid::AndroidRangeType() const {
  return ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT;
}

int BrowserAccessibilityAndroid::RowCount() const {
  if (GetRole() == ui::AX_ROLE_GRID ||
      GetRole() == ui::AX_ROLE_TABLE) {
    return CountChildrenWithRole(ui::AX_ROLE_ROW);
  }

  if (GetRole() == ui::AX_ROLE_LIST ||
      GetRole() == ui::AX_ROLE_LIST_BOX ||
      GetRole() == ui::AX_ROLE_DESCRIPTION_LIST ||
      GetRole() == ui::AX_ROLE_TREE) {
    return PlatformChildCount();
  }

  return 0;
}

int BrowserAccessibilityAndroid::ColumnCount() const {
  if (GetRole() == ui::AX_ROLE_GRID ||
      GetRole() == ui::AX_ROLE_TABLE) {
    return CountChildrenWithRole(ui::AX_ROLE_COLUMN);
  }
  return 0;
}

int BrowserAccessibilityAndroid::RowIndex() const {
  if (GetRole() == ui::AX_ROLE_LIST_ITEM ||
      GetRole() == ui::AX_ROLE_LIST_BOX_OPTION ||
      GetRole() == ui::AX_ROLE_TREE_ITEM) {
    return GetIndexInParent();
  }

  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_INDEX);
}

int BrowserAccessibilityAndroid::RowSpan() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_SPAN);
}

int BrowserAccessibilityAndroid::ColumnIndex() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX);
}

int BrowserAccessibilityAndroid::ColumnSpan() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN);
}

float BrowserAccessibilityAndroid::RangeMin() const {
  return GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE);
}

float BrowserAccessibilityAndroid::RangeMax() const {
  return GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE);
}

float BrowserAccessibilityAndroid::RangeCurrentValue() const {
  return GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE);
}

void BrowserAccessibilityAndroid::GetGranularityBoundaries(
    int granularity,
    std::vector<int32>* starts,
    std::vector<int32>* ends,
    int offset) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE:
      GetLineBoundaries(starts, ends, offset);
      break;
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
      GetWordBoundaries(starts, ends, offset);
      break;
    default:
      NOTREACHED();
  }
}

void BrowserAccessibilityAndroid::GetLineBoundaries(
    std::vector<int32>* line_starts,
    std::vector<int32>* line_ends,
    int offset) {
  // If this node has no children, treat it as all one line.
  if (GetText().size() > 0 && !InternalChildCount()) {
    line_starts->push_back(offset);
    line_ends->push_back(offset + GetText().size());
  }

  // If this is a static text node, get the line boundaries from the
  // inline text boxes if possible.
  if (GetRole() == ui::AX_ROLE_STATIC_TEXT) {
    int last_y = 0;
    for (uint32 i = 0; i < InternalChildCount(); i++) {
      BrowserAccessibilityAndroid* child =
          static_cast<BrowserAccessibilityAndroid*>(InternalGetChild(i));
      CHECK_EQ(ui::AX_ROLE_INLINE_TEXT_BOX, child->GetRole());
      // TODO(dmazzoni): replace this with a proper API to determine
      // if two inline text boxes are on the same line. http://crbug.com/421771
      int y = child->GetLocation().y();
      if (i == 0) {
        line_starts->push_back(offset);
      } else if (y != last_y) {
        line_ends->push_back(offset);
        line_starts->push_back(offset);
      }
      offset += child->GetText().size();
      last_y = y;
    }
    line_ends->push_back(offset);
    return;
  }

  // Otherwise, call GetLineBoundaries recursively on the children.
  for (uint32 i = 0; i < InternalChildCount(); i++) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(InternalGetChild(i));
    child->GetLineBoundaries(line_starts, line_ends, offset);
    offset += child->GetText().size();
  }
}

void BrowserAccessibilityAndroid::GetWordBoundaries(
    std::vector<int32>* word_starts,
    std::vector<int32>* word_ends,
    int offset) {
  if (GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX) {
    const std::vector<int32>& starts = GetIntListAttribute(
        ui::AX_ATTR_WORD_STARTS);
    const std::vector<int32>& ends = GetIntListAttribute(
        ui::AX_ATTR_WORD_ENDS);
    for (size_t i = 0; i < starts.size(); ++i) {
      word_starts->push_back(offset + starts[i]);
      word_ends->push_back(offset + ends[i]);
    }
    return;
  }

  base::string16 concatenated_text;
  for (uint32 i = 0; i < InternalChildCount(); i++) {
    BrowserAccessibilityAndroid* child =
        static_cast<BrowserAccessibilityAndroid*>(InternalGetChild(i));
    base::string16 child_text = child->GetText();
    concatenated_text += child->GetText();
  }

  base::string16 text = GetText();
  if (text.empty() || concatenated_text == text) {
    // Great - this node is just the concatenation of its children, so
    // we can get the word boundaries recursively.
    for (uint32 i = 0; i < InternalChildCount(); i++) {
      BrowserAccessibilityAndroid* child =
          static_cast<BrowserAccessibilityAndroid*>(InternalGetChild(i));
      child->GetWordBoundaries(word_starts, word_ends, offset);
      offset += child->GetText().size();
    }
  } else {
    // This node has its own accessible text that doesn't match its
    // visible text - like alt text for an image or something with an
    // aria-label, so split the text into words locally.
    base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
    if (!iter.Init())
      return;
    while (iter.Advance()) {
      if (iter.IsWord()) {
        word_starts->push_back(iter.prev());
        word_ends->push_back(iter.pos());
      }
    }
  }
}

bool BrowserAccessibilityAndroid::HasFocusableChild() const {
  // This is called from PlatformIsLeaf, so don't call PlatformChildCount
  // from within this!
  for (uint32 i = 0; i < InternalChildCount(); i++) {
    BrowserAccessibility* child = InternalGetChild(i);
    if (child->HasState(ui::AX_STATE_FOCUSABLE))
      return true;
    if (static_cast<BrowserAccessibilityAndroid*>(child)->HasFocusableChild())
      return true;
  }
  return false;
}

bool BrowserAccessibilityAndroid::HasOnlyStaticTextChildren() const {
  // This is called from PlatformIsLeaf, so don't call PlatformChildCount
  // from within this!
  for (uint32 i = 0; i < InternalChildCount(); i++) {
    BrowserAccessibility* child = InternalGetChild(i);
    if (child->GetRole() != ui::AX_ROLE_STATIC_TEXT)
      return false;
  }
  return true;
}

bool BrowserAccessibilityAndroid::HasOnlyTextAndImageChildren() const {
  // This is called from PlatformIsLeaf, so don't call PlatformChildCount
  // from within this!
  for (uint32 i = 0; i < InternalChildCount(); i++) {
    BrowserAccessibility* child = InternalGetChild(i);
    if (child->GetRole() != ui::AX_ROLE_STATIC_TEXT &&
        child->GetRole() != ui::AX_ROLE_IMAGE) {
      return false;
    }
  }
  return true;
}

bool BrowserAccessibilityAndroid::IsIframe() const {
  base::string16 html_tag = GetString16Attribute(
      ui::AX_ATTR_HTML_TAG);
  return html_tag == base::ASCIIToUTF16("iframe");
}

void BrowserAccessibilityAndroid::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();

  if (IsEditableText()) {
    base::string16 value = GetString16Attribute(ui::AX_ATTR_VALUE);
    if (value != new_value_) {
      old_value_ = new_value_;
      new_value_ = value;
    }
  }

  if (GetRole() == ui::AX_ROLE_ALERT && first_time_)
    manager()->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, this);

  base::string16 live;
  if (GetString16Attribute(
      ui::AX_ATTR_CONTAINER_LIVE_STATUS, &live)) {
    NotifyLiveRegionUpdate(live);
  }

  first_time_ = false;
}

void BrowserAccessibilityAndroid::NotifyLiveRegionUpdate(
    base::string16& aria_live) {
  if (!EqualsASCII(aria_live, aria_strings::kAriaLivePolite) &&
      !EqualsASCII(aria_live, aria_strings::kAriaLiveAssertive))
    return;

  base::string16 text = GetText();
  if (cached_text_ != text) {
    if (!text.empty()) {
      manager()->NotifyAccessibilityEvent(ui::AX_EVENT_SHOW,
                                         this);
    }
    cached_text_ = text;
  }
}

int BrowserAccessibilityAndroid::CountChildrenWithRole(ui::AXRole role) const {
  int count = 0;
  for (uint32 i = 0; i < PlatformChildCount(); i++) {
    if (PlatformGetChild(i)->GetRole() == role)
      count++;
  }
  return count;
}

}  // namespace content
