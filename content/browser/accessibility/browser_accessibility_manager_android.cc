// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_android.h"

#include <stddef.h>

#include <cmath>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/i18n/char_iterator.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/common/accessibility_messages.h"
#include "content/public/common/content_features.h"
#include "jni/BrowserAccessibilityManager_jni.h"
#include "ui/accessibility/ax_text_utils.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
//
// Note: The string names for these enums must correspond with the names of
// constants from AccessibilityEvent and AccessibilityServiceInfo, defined
// below. For example, UMA_EVENT_ANNOUNCEMENT corresponds to
// ACCESSIBILITYEVENT_TYPE_ANNOUNCEMENT via the macro
// EVENT_TYPE_HISTOGRAM(event_type_mask, ANNOUNCEMENT).
enum {
  UMA_CAPABILITY_CAN_CONTROL_MAGNIFICATION = 0,
  UMA_CAPABILITY_CAN_PERFORM_GESTURES = 1,
  UMA_CAPABILITY_CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 2,
  UMA_CAPABILITY_CAN_REQUEST_FILTER_KEY_EVENTS = 3,
  UMA_CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION = 4,
  UMA_CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT = 5,
  UMA_EVENT_ANNOUNCEMENT = 6,
  UMA_EVENT_ASSIST_READING_CONTEXT = 7,
  UMA_EVENT_GESTURE_DETECTION_END = 8,
  UMA_EVENT_GESTURE_DETECTION_START = 9,
  UMA_EVENT_NOTIFICATION_STATE_CHANGED = 10,
  UMA_EVENT_TOUCH_EXPLORATION_GESTURE_END = 11,
  UMA_EVENT_TOUCH_EXPLORATION_GESTURE_START = 12,
  UMA_EVENT_TOUCH_INTERACTION_END = 13,
  UMA_EVENT_TOUCH_INTERACTION_START = 14,
  UMA_EVENT_VIEW_ACCESSIBILITY_FOCUSED = 15,
  UMA_EVENT_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 16,
  UMA_EVENT_VIEW_CLICKED = 17,
  UMA_EVENT_VIEW_CONTEXT_CLICKED = 18,
  UMA_EVENT_VIEW_FOCUSED = 19,
  UMA_EVENT_VIEW_HOVER_ENTER = 20,
  UMA_EVENT_VIEW_HOVER_EXIT = 21,
  UMA_EVENT_VIEW_LONG_CLICKED = 22,
  UMA_EVENT_VIEW_SCROLLED = 23,
  UMA_EVENT_VIEW_SELECTED = 24,
  UMA_EVENT_VIEW_TEXT_CHANGED = 25,
  UMA_EVENT_VIEW_TEXT_SELECTION_CHANGED = 26,
  UMA_EVENT_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 27,
  UMA_EVENT_WINDOWS_CHANGED = 28,
  UMA_EVENT_WINDOW_CONTENT_CHANGED = 29,
  UMA_EVENT_WINDOW_STATE_CHANGED = 30,
  UMA_FEEDBACK_AUDIBLE = 31,
  UMA_FEEDBACK_BRAILLE = 32,
  UMA_FEEDBACK_GENERIC = 33,
  UMA_FEEDBACK_HAPTIC = 34,
  UMA_FEEDBACK_SPOKEN = 35,
  UMA_FEEDBACK_VISUAL = 36,
  UMA_FLAG_FORCE_DIRECT_BOOT_AWARE = 37,
  UMA_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 38,
  UMA_FLAG_REPORT_VIEW_IDS = 39,
  UMA_FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 40,
  UMA_FLAG_REQUEST_FILTER_KEY_EVENTS = 41,
  UMA_FLAG_REQUEST_TOUCH_EXPLORATION_MODE = 42,
  UMA_FLAG_RETRIEVE_INTERACTIVE_WINDOWS = 43,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  UMA_ACCESSIBILITYSERVICEINFO_MAX
};

// These are constants from
// android.view.accessibility.AccessibilityEvent in Java.
//
// If you add a new constant, add a new UMA enum above and add a line
// to CollectStats(), below.
enum {
  ACCESSIBILITYEVENT_TYPE_VIEW_CLICKED = 0x00000001,
  ACCESSIBILITYEVENT_TYPE_VIEW_LONG_CLICKED = 0x00000002,
  ACCESSIBILITYEVENT_TYPE_VIEW_SELECTED = 0x00000004,
  ACCESSIBILITYEVENT_TYPE_VIEW_FOCUSED = 0x00000008,
  ACCESSIBILITYEVENT_TYPE_VIEW_TEXT_CHANGED = 0x00000010,
  ACCESSIBILITYEVENT_TYPE_WINDOW_STATE_CHANGED = 0x00000020,
  ACCESSIBILITYEVENT_TYPE_NOTIFICATION_STATE_CHANGED = 0x00000040,
  ACCESSIBILITYEVENT_TYPE_VIEW_HOVER_ENTER = 0x00000080,
  ACCESSIBILITYEVENT_TYPE_VIEW_HOVER_EXIT = 0x00000100,
  ACCESSIBILITYEVENT_TYPE_TOUCH_EXPLORATION_GESTURE_START = 0x00000200,
  ACCESSIBILITYEVENT_TYPE_TOUCH_EXPLORATION_GESTURE_END = 0x00000400,
  ACCESSIBILITYEVENT_TYPE_WINDOW_CONTENT_CHANGED = 0x00000800,
  ACCESSIBILITYEVENT_TYPE_VIEW_SCROLLED = 0x00001000,
  ACCESSIBILITYEVENT_TYPE_VIEW_TEXT_SELECTION_CHANGED = 0x00002000,
  ACCESSIBILITYEVENT_TYPE_ANNOUNCEMENT = 0x00004000,
  ACCESSIBILITYEVENT_TYPE_VIEW_ACCESSIBILITY_FOCUSED = 0x00008000,
  ACCESSIBILITYEVENT_TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 0x00010000,
  ACCESSIBILITYEVENT_TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY =
      0x00020000,
  ACCESSIBILITYEVENT_TYPE_GESTURE_DETECTION_START = 0x00040000,
  ACCESSIBILITYEVENT_TYPE_GESTURE_DETECTION_END = 0x00080000,
  ACCESSIBILITYEVENT_TYPE_TOUCH_INTERACTION_START = 0x00100000,
  ACCESSIBILITYEVENT_TYPE_TOUCH_INTERACTION_END = 0x00200000,
  ACCESSIBILITYEVENT_TYPE_WINDOWS_CHANGED = 0x00400000,
  ACCESSIBILITYEVENT_TYPE_VIEW_CONTEXT_CLICKED = 0x00800000,
  ACCESSIBILITYEVENT_TYPE_ASSIST_READING_CONTEXT = 0x01000000,
};

// These are constants from
// android.accessibilityservice.AccessibilityServiceInfo in Java:
//
// If you add a new constant, add a new UMA enum above and add a line
// to CollectStats(), below.
enum {
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT = 0x00000001,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION =
      0x00000002,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY =
      0x00000004,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_REQUEST_FILTER_KEY_EVENTS =
      0x00000008,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_CONTROL_MAGNIFICATION = 0x00000010,
  ACCESSIBILITYSERVICEINFO_CAPABILITY_CAN_PERFORM_GESTURES = 0x00000020,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_SPOKEN = 0x0000001,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_HAPTIC = 0x0000002,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_AUDIBLE = 0x0000004,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_VISUAL = 0x0000008,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_GENERIC = 0x0000010,
  ACCESSIBILITYSERVICEINFO_FEEDBACK_BRAILLE = 0x0000020,
  ACCESSIBILITYSERVICEINFO_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 0x0000002,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_TOUCH_EXPLORATION_MODE = 0x0000004,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 0x00000008,
  ACCESSIBILITYSERVICEINFO_FLAG_REPORT_VIEW_IDS = 0x00000010,
  ACCESSIBILITYSERVICEINFO_FLAG_REQUEST_FILTER_KEY_EVENTS = 0x00000020,
  ACCESSIBILITYSERVICEINFO_FLAG_RETRIEVE_INTERACTIVE_WINDOWS = 0x00000040,
  ACCESSIBILITYSERVICEINFO_FLAG_FORCE_DIRECT_BOOT_AWARE = 0x00010000,
};

// These macros simplify recording a histogram based on information we get
// from an AccessibilityService. There are four bitmasks of information
// we get from each AccessibilityService, and for each possible bit mask
// corresponding to one flag, we want to check if that flag is set, and if
// so, record the corresponding histogram.
//
// Doing this with macros reduces the chance for human error by
// recording the wrong histogram for the wrong flag.
//
// These macros are used by CollectStats(), below.
#define EVENT_TYPE_HISTOGRAM(event_type_mask, event_type)       \
  if (event_type_mask & ACCESSIBILITYEVENT_TYPE_##event_type)   \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo", \
                            UMA_EVENT_##event_type,             \
                            UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define FLAGS_HISTOGRAM(flags_mask, flag)                       \
  if (flags_mask & ACCESSIBILITYSERVICEINFO_FLAG_##flag)        \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo", \
                            UMA_FLAG_##flag, UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, feedback_type)            \
  if (feedback_type_mask & ACCESSIBILITYSERVICEINFO_FEEDBACK_##feedback_type) \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo",               \
                            UMA_FEEDBACK_##feedback_type,                     \
                            UMA_ACCESSIBILITYSERVICEINFO_MAX)
#define CAPABILITY_TYPE_HISTOGRAM(capability_type_mask, capability_type) \
  if (capability_type_mask &                                             \
      ACCESSIBILITYSERVICEINFO_CAPABILITY_##capability_type)             \
  UMA_HISTOGRAM_ENUMERATION("Accessibility.AndroidServiceInfo",          \
                            UMA_CAPABILITY_##capability_type,            \
                            UMA_ACCESSIBILITYSERVICEINFO_MAX)

using SearchKeyToPredicateMap =
    base::hash_map<base::string16, AccessibilityMatchPredicate>;
base::LazyInstance<SearchKeyToPredicateMap>::DestructorAtExit
    g_search_key_to_predicate_map = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::string16>::DestructorAtExit g_all_search_keys =
    LAZY_INSTANCE_INITIALIZER;

bool SectionPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ui::AX_ROLE_ARTICLE:
    case ui::AX_ROLE_APPLICATION:
    case ui::AX_ROLE_BANNER:
    case ui::AX_ROLE_COMPLEMENTARY:
    case ui::AX_ROLE_CONTENT_INFO:
    case ui::AX_ROLE_HEADING:
    case ui::AX_ROLE_MAIN:
    case ui::AX_ROLE_NAVIGATION:
    case ui::AX_ROLE_SEARCH:
    case ui::AX_ROLE_REGION:
      return true;
    default:
      return false;
  }
}

bool AllInterestingNodesPredicate(
    BrowserAccessibility* start, BrowserAccessibility* node) {
  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  return android_node->IsInterestingOnAndroid();
}

void AddToPredicateMap(const char* search_key_ascii,
                       AccessibilityMatchPredicate predicate) {
  base::string16 search_key_utf16 = base::ASCIIToUTF16(search_key_ascii);
  g_search_key_to_predicate_map.Get()[search_key_utf16] = predicate;
  if (!g_all_search_keys.Get().empty())
    g_all_search_keys.Get() += base::ASCIIToUTF16(",");
  g_all_search_keys.Get() += search_key_utf16;
}

// These are special unofficial strings sent from TalkBack/BrailleBack
// to jump to certain categories of web elements.
void InitSearchKeyToPredicateMapIfNeeded() {
  if (!g_search_key_to_predicate_map.Get().empty())
    return;

  AddToPredicateMap("ARTICLE", AccessibilityArticlePredicate);
  AddToPredicateMap("BUTTON", AccessibilityButtonPredicate);
  AddToPredicateMap("CHECKBOX", AccessibilityCheckboxPredicate);
  AddToPredicateMap("COMBOBOX", AccessibilityComboboxPredicate);
  AddToPredicateMap("CONTROL", AccessibilityControlPredicate);
  AddToPredicateMap("FOCUSABLE", AccessibilityFocusablePredicate);
  AddToPredicateMap("FRAME", AccessibilityFramePredicate);
  AddToPredicateMap("GRAPHIC", AccessibilityGraphicPredicate);
  AddToPredicateMap("H1", AccessibilityH1Predicate);
  AddToPredicateMap("H2", AccessibilityH2Predicate);
  AddToPredicateMap("H3", AccessibilityH3Predicate);
  AddToPredicateMap("H4", AccessibilityH4Predicate);
  AddToPredicateMap("H5", AccessibilityH5Predicate);
  AddToPredicateMap("H6", AccessibilityH6Predicate);
  AddToPredicateMap("HEADING", AccessibilityHeadingPredicate);
  AddToPredicateMap("LANDMARK", AccessibilityLandmarkPredicate);
  AddToPredicateMap("LINK", AccessibilityLinkPredicate);
  AddToPredicateMap("LIST", AccessibilityListPredicate);
  AddToPredicateMap("LIST_ITEM", AccessibilityListItemPredicate);
  AddToPredicateMap("MAIN", AccessibilityMainPredicate);
  AddToPredicateMap("MEDIA", AccessibilityMediaPredicate);
  AddToPredicateMap("RADIO", AccessibilityRadioButtonPredicate);
  AddToPredicateMap("SECTION", SectionPredicate);
  AddToPredicateMap("TABLE", AccessibilityTablePredicate);
  AddToPredicateMap("TEXT_FIELD", AccessibilityTextfieldPredicate);
  AddToPredicateMap("UNVISITED_LINK", AccessibilityUnvisitedLinkPredicate);
  AddToPredicateMap("VISITED_LINK", AccessibilityVisitedLinkPredicate);
}

AccessibilityMatchPredicate PredicateForSearchKey(
    const base::string16& element_type) {
  InitSearchKeyToPredicateMapIfNeeded();
  const auto& iter = g_search_key_to_predicate_map.Get().find(element_type);
  if (iter != g_search_key_to_predicate_map.Get().end())
    return iter->second;

  // If we don't recognize the selector, return any element that a
  // screen reader should navigate to.
  return AllInterestingNodesPredicate;
}

// The element in the document for which we may be displaying an autofill popup.
int32_t g_element_hosting_autofill_popup_unique_id = -1;

// The element in the document that is the next element after
// |g_element_hosting_autofill_popup_unique_id|.
int32_t g_element_after_element_hosting_autofill_popup_unique_id = -1;

// Autofill popup will not be part of the |AXTree| that is sent by renderer.
// Hence, we need a proxy |AXNode| to represent the autofill popup.
BrowserAccessibility* g_autofill_popup_proxy_node = nullptr;
ui::AXNode* g_autofill_popup_proxy_node_ax_node = nullptr;

void DeleteAutofillPopupProxy() {
  if (g_autofill_popup_proxy_node) {
    g_autofill_popup_proxy_node->Destroy();
    delete g_autofill_popup_proxy_node_ax_node;
    g_autofill_popup_proxy_node = nullptr;
  }
}

}  // anonymous namespace

namespace aria_strings {
  const char kAriaLivePolite[] = "polite";
  const char kAriaLiveAssertive[] = "assertive";
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerAndroid(
      ScopedJavaLocalRef<jobject>(), initial_tree, delegate, factory);
}

BrowserAccessibilityManagerAndroid*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerAndroid() {
  return static_cast<BrowserAccessibilityManagerAndroid*>(this);
}

BrowserAccessibilityManagerAndroid::BrowserAccessibilityManagerAndroid(
    ScopedJavaLocalRef<jobject> content_view_core,
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      prune_tree_for_screen_reader_(true) {
  Initialize(initial_tree);
  SetContentViewCore(content_view_core);

  // TODO(dmazzoni): get rid of the use of this class by
  // AXTreeSnapshotCallback so that content_view_core is always valid.
  if (!content_view_core.is_null())
    CollectStats();
}

BrowserAccessibilityManagerAndroid::~BrowserAccessibilityManagerAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return;

  // Clean up autofill popup proxy node in case the popup was not dismissed.
  DeleteAutofillPopupProxy();

  Java_BrowserAccessibilityManager_onNativeObjectDestroyed(
      env, obj, reinterpret_cast<intptr_t>(this));
}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerAndroid::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.AddState(ui::AX_STATE_READ_ONLY);

  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerAndroid::SetContentViewCore(
    ScopedJavaLocalRef<jobject> content_view_core) {
  if (content_view_core.is_null())
    return;

  JNIEnv* env = AttachCurrentThread();
  java_ref_ = JavaObjectWeakGlobalRef(
      env, Java_BrowserAccessibilityManager_create(
               env, reinterpret_cast<intptr_t>(this), content_view_core)
               .obj());
}

bool BrowserAccessibilityManagerAndroid::ShouldRespectDisplayedPasswordText() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return false;

  return Java_BrowserAccessibilityManager_shouldRespectDisplayedPasswordText(
      env, obj);
}

bool BrowserAccessibilityManagerAndroid::ShouldExposePasswordText() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return false;

  return Java_BrowserAccessibilityManager_shouldExposePasswordText(env, obj);
}

BrowserAccessibility* BrowserAccessibilityManagerAndroid::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerAndroid::NotifyAccessibilityEvent(
    BrowserAccessibilityEvent::Source source,
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return;

  if (event_type == ui::AX_EVENT_HIDE)
    return;

  if (event_type == ui::AX_EVENT_TREE_CHANGED)
    return;

  // Layout changes are handled in OnLocationChanges and
  // SendLocationChangeEvents.
  if (event_type == ui::AX_EVENT_LAYOUT_COMPLETE)
    return;

  if (event_type == ui::AX_EVENT_HOVER) {
    HandleHoverEvent(node);
    return;
  }

  // Sometimes we get events on nodes in our internal accessibility tree
  // that aren't exposed on Android. Update |node| to point to the highest
  // ancestor that's a leaf node.
  BrowserAccessibility* original_node = node;
  node = node->GetClosestPlatformObject();
  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  // If the closest platform object is a password field, the event we're
  // getting is doing something in the shadow dom, for example replacing a
  // character with a dot after a short pause. On Android we don't want to
  // fire an event for those changes, but we do want to make sure our internal
  // state is correct, so we call OnDataChanged() and then return.
  if (android_node->IsPassword() && original_node != node) {
    android_node->OnDataChanged();
    return;
  }

  // Always send AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED to notify
  // the Android system that the accessibility hierarchy rooted at this
  // node has changed.
  Java_BrowserAccessibilityManager_handleContentChanged(env, obj,
                                                        node->unique_id());

  // Ignore load complete events on iframes.
  if (event_type == ui::AX_EVENT_LOAD_COMPLETE &&
      node->manager() != GetRootManager()) {
    return;
  }

  switch (event_type) {
    case ui::AX_EVENT_LOAD_COMPLETE:
      Java_BrowserAccessibilityManager_handlePageLoaded(
          env, obj, GetFocus()->unique_id());
      break;
    case ui::AX_EVENT_FOCUS:
      Java_BrowserAccessibilityManager_handleFocusChanged(env, obj,
                                                          node->unique_id());
      break;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      Java_BrowserAccessibilityManager_handleCheckStateChanged(
          env, obj, node->unique_id());
      break;
    case ui::AX_EVENT_CLICKED:
      Java_BrowserAccessibilityManager_handleClicked(env, obj,
                                                     node->unique_id());
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      Java_BrowserAccessibilityManager_handleScrollPositionChanged(
          env, obj, node->unique_id());
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      Java_BrowserAccessibilityManager_handleScrolledToAnchor(
          env, obj, node->unique_id());
      break;
    case ui::AX_EVENT_ALERT:
      // An alert is a special case of live region. Fall through to the
      // next case to handle it.
    case ui::AX_EVENT_SHOW: {
      // This event is fired when an object appears in a live region.
      // Speak its text.
      Java_BrowserAccessibilityManager_announceLiveRegionText(
          env, obj, base::android::ConvertUTF16ToJavaString(
                        env, android_node->GetText()));
      break;
    }
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
      Java_BrowserAccessibilityManager_handleTextSelectionChanged(
          env, obj, node->unique_id());
      break;
    case ui::AX_EVENT_TEXT_CHANGED:
    case ui::AX_EVENT_VALUE_CHANGED:
      if (android_node->IsEditableText() && GetFocus() == node) {
        Java_BrowserAccessibilityManager_handleEditableTextChanged(
            env, obj, node->unique_id());
      } else if (android_node->IsSlider()) {
        Java_BrowserAccessibilityManager_handleSliderChanged(env, obj,
                                                             node->unique_id());
      }
      break;
    default:
      // There are some notifications that aren't meaningful on Android.
      // It's okay to skip them.
      break;
  }
}

void BrowserAccessibilityManagerAndroid::SendLocationChangeEvents(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  // Android is not very efficient at handling notifications, and location
  // changes in particular are frequent and not time-critical. If a lot of
  // nodes changed location, just send a single notification after a short
  // delay (to batch them), rather than lots of individual notifications.
  if (params.size() > 3) {
    ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
    JNIEnv* env = AttachCurrentThread();
    if (obj.is_null())
      return;
    Java_BrowserAccessibilityManager_sendDelayedWindowContentChangedEvent(env,
                                                                          obj);
    return;
  }

  BrowserAccessibilityManager::SendLocationChangeEvents(params);
}

base::android::ScopedJavaLocalRef<jstring>
BrowserAccessibilityManagerAndroid::GetSupportedHtmlElementTypes(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  InitSearchKeyToPredicateMapIfNeeded();
  return base::android::ConvertUTF16ToJavaString(env, g_all_search_keys.Get());
}

jint BrowserAccessibilityManagerAndroid::GetRootId(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (GetRoot())
    return static_cast<jint>(GetRoot()->unique_id());
  else
    return -1;
}

jboolean BrowserAccessibilityManagerAndroid::IsNodeValid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  return GetFromUniqueID(id) != NULL;
}

void BrowserAccessibilityManagerAndroid::HitTest(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint x,
    jint y) {
  BrowserAccessibilityManager::HitTest(gfx::Point(x, y));
}

jboolean BrowserAccessibilityManagerAndroid::IsEditableText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->IsEditableText();
}

jboolean BrowserAccessibilityManagerAndroid::IsFocused(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->IsFocused();
}

jint BrowserAccessibilityManagerAndroid::GetEditableTextSelectionStart(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START);
}

jint BrowserAccessibilityManagerAndroid::GetEditableTextSelectionEnd(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END);
}

jboolean BrowserAccessibilityManagerAndroid::PopulateAccessibilityNodeInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& info,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  if (node->PlatformGetParent()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoParent(
        env, obj, info, node->PlatformGetParent()->unique_id());
  }
  for (unsigned i = 0; i < node->PlatformChildCount(); ++i) {
    Java_BrowserAccessibilityManager_addAccessibilityNodeInfoChild(
        env, obj, info, node->PlatformGetChild(i)->unique_id());
  }
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoBooleanAttributes(
      env, obj, info,
      id,
      node->IsCheckable(),
      node->IsChecked(),
      node->IsClickable(),
      node->IsEnabled(),
      node->IsFocusable(),
      node->IsFocused(),
      node->IsPassword(),
      node->IsScrollable(),
      node->IsSelected(),
      node->IsVisibleToUser());
  Java_BrowserAccessibilityManager_addAccessibilityNodeInfoActions(
      env, obj, info,
      id,
      node->CanScrollForward(),
      node->CanScrollBackward(),
      node->CanScrollUp(),
      node->CanScrollDown(),
      node->CanScrollLeft(),
      node->CanScrollRight(),
      node->IsClickable(),
      node->IsEditableText(),
      node->IsEnabled(),
      node->IsFocusable(),
      node->IsFocused(),
      node->IsCollapsed(),
      node->IsExpanded(),
      node->HasNonEmptyValue());
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoClassName(
      env, obj, info,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()));
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoText(
      env, obj, info,
      base::android::ConvertUTF16ToJavaString(env, node->GetText()),
      node->IsLink(), node->IsEditableText(),
      base::android::ConvertUTF16ToJavaString(
          env, node->GetInheritedString16Attribute(ui::AX_ATTR_LANGUAGE)));
  base::string16 element_id;
  if (node->GetHtmlAttribute("id", &element_id)) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoViewIdResourceName(
        env, obj, info,
        base::android::ConvertUTF16ToJavaString(env, element_id));
  }

  gfx::Rect absolute_rect = node->GetPageBoundsRect();
  gfx::Rect parent_relative_rect = absolute_rect;
  if (node->PlatformGetParent()) {
    gfx::Rect parent_rect = node->PlatformGetParent()->GetPageBoundsRect();
    parent_relative_rect.Offset(-parent_rect.OffsetFromOrigin());
  }
  bool is_root = node->PlatformGetParent() == NULL;
  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoLocation(
      env, obj, info,
      id,
      absolute_rect.x(), absolute_rect.y(),
      parent_relative_rect.x(), parent_relative_rect.y(),
      absolute_rect.width(), absolute_rect.height(),
      is_root);

  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoKitKatAttributes(
      env, obj, info, is_root, node->IsEditableText(),
      base::android::ConvertUTF16ToJavaString(env, node->GetRoleDescription()),
      node->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START),
      node->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END));

  Java_BrowserAccessibilityManager_setAccessibilityNodeInfoLollipopAttributes(
      env, obj, info,
      node->CanOpenPopup(),
      node->IsContentInvalid(),
      node->IsDismissable(),
      node->IsMultiLine(),
      node->AndroidInputType(),
      node->AndroidLiveRegionType());
  if (node->IsCollection()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoCollectionInfo(
        env, obj, info,
        node->RowCount(),
        node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsCollectionItem() || node->IsHeading()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoCollectionItemInfo(
        env, obj, info,
        node->RowIndex(),
        node->RowSpan(),
        node->ColumnIndex(),
        node->ColumnSpan(),
        node->IsHeading());
  }
  if (node->IsRangeType()) {
    Java_BrowserAccessibilityManager_setAccessibilityNodeInfoRangeInfo(
        env, obj, info,
        node->AndroidRangeType(),
        node->RangeMin(),
        node->RangeMax(),
        node->RangeCurrentValue());
  }

  return true;
}

jboolean BrowserAccessibilityManagerAndroid::PopulateAccessibilityEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& event,
    jint id,
    jint event_type) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  Java_BrowserAccessibilityManager_setAccessibilityEventBooleanAttributes(
      env, obj, event,
      node->IsChecked(),
      node->IsEnabled(),
      node->IsPassword(),
      node->IsScrollable());
  Java_BrowserAccessibilityManager_setAccessibilityEventClassName(
      env, obj, event,
      base::android::ConvertUTF8ToJavaString(env, node->GetClassName()));
  Java_BrowserAccessibilityManager_setAccessibilityEventListAttributes(
      env, obj, event,
      node->GetItemIndex(),
      node->GetItemCount());
  Java_BrowserAccessibilityManager_setAccessibilityEventScrollAttributes(
      env, obj, event,
      node->GetScrollX(),
      node->GetScrollY(),
      node->GetMaxScrollX(),
      node->GetMaxScrollY());

  switch (event_type) {
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_CHANGED: {
      base::string16 before_text = node->GetTextChangeBeforeText();
      base::string16 text = node->GetText();
      Java_BrowserAccessibilityManager_setAccessibilityEventTextChangedAttrs(
          env, obj, event, node->GetTextChangeFromIndex(),
          node->GetTextChangeAddedCount(), node->GetTextChangeRemovedCount(),
          base::android::ConvertUTF16ToJavaString(env, before_text),
          base::android::ConvertUTF16ToJavaString(env, text));
      break;
    }
    case ANDROID_ACCESSIBILITY_EVENT_TEXT_SELECTION_CHANGED: {
      base::string16 text = node->GetText();
      Java_BrowserAccessibilityManager_setAccessibilityEventSelectionAttrs(
          env, obj, event, node->GetSelectionStart(), node->GetSelectionEnd(),
          node->GetEditableTextLength(),
          base::android::ConvertUTF16ToJavaString(env, text));
      break;
    }
    default:
      break;
  }

  Java_BrowserAccessibilityManager_setAccessibilityEventLollipopAttributes(
      env, obj, event,
      node->CanOpenPopup(),
      node->IsContentInvalid(),
      node->IsDismissable(),
      node->IsMultiLine(),
      node->AndroidInputType(),
      node->AndroidLiveRegionType());
  if (node->IsCollection()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventCollectionInfo(
        env, obj, event,
        node->RowCount(),
        node->ColumnCount(),
        node->IsHierarchical());
  }
  if (node->IsHeading()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventHeadingFlag(
        env, obj, event, true);
  }
  if (node->IsCollectionItem()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventCollectionItemInfo(
        env, obj, event,
        node->RowIndex(),
        node->RowSpan(),
        node->ColumnIndex(),
        node->ColumnSpan());
  }
  if (node->IsRangeType()) {
    Java_BrowserAccessibilityManager_setAccessibilityEventRangeInfo(
        env, obj, event,
        node->AndroidRangeType(),
        node->RangeMin(),
        node->RangeMax(),
        node->RangeCurrentValue());
  }

  return true;
}

void BrowserAccessibilityManagerAndroid::Click(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    node->manager()->DoDefaultAction(*node);
}

void BrowserAccessibilityManagerAndroid::Focus(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    node->manager()->SetFocus(*node);
}

void BrowserAccessibilityManagerAndroid::Blur(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  SetFocus(*GetRoot());
}

void BrowserAccessibilityManagerAndroid::ScrollToMakeNodeVisible(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    node->manager()->ScrollToMakeVisible(
        *node, gfx::Rect(node->GetFrameBoundsRect().size()));
}

void BrowserAccessibilityManagerAndroid::SetTextFieldValue(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    const JavaParamRef<jstring>& value) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node) {
    node->manager()->SetValue(
        *node, base::android::ConvertJavaStringToUTF16(env, value));
  }
}

void BrowserAccessibilityManagerAndroid::SetSelection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    jint start,
    jint end) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node) {
    node->manager()->SetSelection(AXPlatformRange(node->CreatePositionAt(start),
                                                  node->CreatePositionAt(end)));
  }
}

jboolean BrowserAccessibilityManagerAndroid::AdjustSlider(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    jboolean increment) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);

  if (!android_node->IsSlider() || !android_node->IsEnabled())
    return false;

  float value = node->GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE);
  float min = node->GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE);
  float max = node->GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE);
  if (max <= min)
    return false;

  // To behave similarly to an Android SeekBar, move by an increment of
  // approximately 20%.
  float original_value = value;
  float delta = (max - min) / 5.0f;
  value += (increment ? delta : -delta);
  value = std::max(std::min(value, max), min);
  if (value != original_value) {
    node->manager()->SetValue(
        *node, base::UTF8ToUTF16(base::DoubleToString(value)));
    return true;
  }
  return false;
}

void BrowserAccessibilityManagerAndroid::ShowContextMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    node->manager()->ShowContextMenu(*node);
}

void BrowserAccessibilityManagerAndroid::HandleHoverEvent(
    BrowserAccessibility* node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return;

  // First walk up to the nearest platform node, in case this node isn't
  // even exposed on the platform.
  node = node->GetClosestPlatformObject();

  // If this node is uninteresting and just a wrapper around a sole
  // interesting descendant, prefer that descendant instead.
  const BrowserAccessibilityAndroid* android_node =
      static_cast<BrowserAccessibilityAndroid*>(node);
  const BrowserAccessibilityAndroid* sole_interesting_node =
      android_node->GetSoleInterestingNodeFromSubtree();
  if (sole_interesting_node)
    android_node = sole_interesting_node;

  // Finally, if this node is still uninteresting, try to walk up to
  // find an interesting parent.
  while (android_node && !android_node->IsInterestingOnAndroid()) {
    android_node = static_cast<BrowserAccessibilityAndroid*>(
        android_node->PlatformGetParent());
  }

  if (android_node) {
    Java_BrowserAccessibilityManager_handleHover(
        env, obj, android_node->unique_id());
  }
}

jint BrowserAccessibilityManagerAndroid::FindElementType(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint start_id,
    const JavaParamRef<jstring>& element_type_str,
    jboolean forwards) {
  BrowserAccessibilityAndroid* start_node = GetFromUniqueID(start_id);
  if (!start_node)
    return 0;

  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (!root_manager)
    return 0;

  BrowserAccessibility* root = root_manager->GetRoot();
  if (!root)
    return 0;

  AccessibilityMatchPredicate predicate = PredicateForSearchKey(
      base::android::ConvertJavaStringToUTF16(env, element_type_str));

  OneShotAccessibilityTreeSearch tree_search(root);
  tree_search.SetStartNode(start_node);
  tree_search.SetDirection(
      forwards ?
          OneShotAccessibilityTreeSearch::FORWARDS :
          OneShotAccessibilityTreeSearch::BACKWARDS);
  tree_search.SetResultLimit(1);
  tree_search.SetImmediateDescendantsOnly(false);
  tree_search.SetVisibleOnly(false);
  tree_search.AddPredicate(predicate);

  if (tree_search.CountMatches() == 0)
    return 0;

  int32_t element_id = tree_search.GetMatchAtIndex(0)->unique_id();

  // Navigate forwards to the autofill popup's proxy node if focus is currently
  // on the element hosting the autofill popup. Once within the popup, a back
  // press will navigate back to the element hosting the popup. If user swipes
  // past last suggestion in the popup, or swipes left from the first suggestion
  // in the popup, we will navigate to the element that is the next element in
  // the document after the element hosting the popup.
  if (forwards && start_id == g_element_hosting_autofill_popup_unique_id &&
      g_autofill_popup_proxy_node) {
    g_element_after_element_hosting_autofill_popup_unique_id = element_id;
    return g_autofill_popup_proxy_node->unique_id();
  }

  return element_id;
}

jboolean BrowserAccessibilityManagerAndroid::NextAtGranularity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint granularity,
    jboolean extend_selection,
    jint id,
    jint cursor_index) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (NextAtGranularity(granularity, cursor_index, node,
                        &start_index, &end_index)) {
    base::string16 text = node->GetText();
    Java_BrowserAccessibilityManager_finishGranularityMove(
        env, obj, base::android::ConvertUTF16ToJavaString(env, text),
        extend_selection, start_index, end_index, true);
    return true;
  }
  return false;
}

jboolean BrowserAccessibilityManagerAndroid::PreviousAtGranularity(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint granularity,
    jboolean extend_selection,
    jint id,
    jint cursor_index) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  jint start_index = -1;
  int end_index = -1;
  if (PreviousAtGranularity(granularity, cursor_index, node,
                            &start_index, &end_index)) {
    Java_BrowserAccessibilityManager_finishGranularityMove(
        env, obj, base::android::ConvertUTF16ToJavaString(env, node->GetText()),
        extend_selection, start_index, end_index, false);
    return true;
  }
  return false;
}

bool BrowserAccessibilityManagerAndroid::NextAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      base::string16 text = node->GetText();
      if (cursor_index >= static_cast<int32_t>(text.length()))
        return false;
      base::i18n::UTF16CharIterator iter(text.data(), text.size());
      while (!iter.end() && iter.array_pos() <= cursor_index)
        iter.Advance();
      *start_index = iter.array_pos();
      *end_index = iter.array_pos();
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0)
        return false;

      size_t index = 0;
      while (index < starts.size() - 1 && starts[index] < cursor_index)
        index++;

      if (starts[index] < cursor_index)
        return false;

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED();
  }

  return true;
}

bool BrowserAccessibilityManagerAndroid::PreviousAtGranularity(
    int32_t granularity,
    int32_t cursor_index,
    BrowserAccessibilityAndroid* node,
    int32_t* start_index,
    int32_t* end_index) {
  switch (granularity) {
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_CHARACTER: {
      if (cursor_index <= 0)
        return false;
      base::string16 text = node->GetText();
      base::i18n::UTF16CharIterator iter(text.data(), text.size());
      int previous_index = 0;
      while (!iter.end() && iter.array_pos() < cursor_index) {
        previous_index = iter.array_pos();
        iter.Advance();
      }
      *start_index = previous_index;
      *end_index = previous_index;
      break;
    }
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_WORD:
    case ANDROID_ACCESSIBILITY_NODE_INFO_MOVEMENT_GRANULARITY_LINE: {
      std::vector<int32_t> starts;
      std::vector<int32_t> ends;
      node->GetGranularityBoundaries(granularity, &starts, &ends, 0);
      if (starts.size() == 0)
        return false;

      size_t index = starts.size() - 1;
      while (index > 0 && starts[index] >= cursor_index)
        index--;

      if (starts[index] >= cursor_index)
        return false;

      *start_index = starts[index];
      *end_index = ends[index];
      break;
    }
    default:
      NOTREACHED();
  }

  return true;
}

void BrowserAccessibilityManagerAndroid::SetAccessibilityFocus(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (node)
    node->manager()->SetAccessibilityFocus(*node);
}

bool BrowserAccessibilityManagerAndroid::IsSlider(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->GetRole() == ui::AX_ROLE_SLIDER;
}

void BrowserAccessibilityManagerAndroid::OnAutofillPopupDisplayed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!base::FeatureList::IsEnabled(features::kAndroidAutofillAccessibility))
    return;

  BrowserAccessibility* current_focus = GetFocus();
  if (current_focus == nullptr) {
    return;
  }

  DeleteAutofillPopupProxy();

  g_autofill_popup_proxy_node = BrowserAccessibility::Create();
  g_autofill_popup_proxy_node_ax_node = new ui::AXNode(nullptr, -1, -1);
  ui::AXNodeData ax_node_data;
  ax_node_data.role = ui::AX_ROLE_MENU;
  ax_node_data.SetName("Autofill");
  ax_node_data.AddState(ui::AX_STATE_READ_ONLY);
  ax_node_data.AddState(ui::AX_STATE_FOCUSABLE);
  ax_node_data.AddState(ui::AX_STATE_SELECTABLE);
  g_autofill_popup_proxy_node_ax_node->SetData(ax_node_data);
  g_autofill_popup_proxy_node->Init(this, g_autofill_popup_proxy_node_ax_node);

  g_element_hosting_autofill_popup_unique_id = current_focus->unique_id();
}

void BrowserAccessibilityManagerAndroid::OnAutofillPopupDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  g_element_hosting_autofill_popup_unique_id = -1;
  g_element_after_element_hosting_autofill_popup_unique_id = -1;
  DeleteAutofillPopupProxy();
}

jint BrowserAccessibilityManagerAndroid::
    GetIdForElementAfterElementHostingAutofillPopup(
        JNIEnv* env,
        const JavaParamRef<jobject>& obj) {
  if (!base::FeatureList::IsEnabled(features::kAndroidAutofillAccessibility) ||
      g_element_after_element_hosting_autofill_popup_unique_id == -1 ||
      GetFromUniqueID(
          g_element_after_element_hosting_autofill_popup_unique_id) == nullptr)
    return 0;

  return g_element_after_element_hosting_autofill_popup_unique_id;
}

jboolean BrowserAccessibilityManagerAndroid::IsAutofillPopupNode(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id) {
  return g_autofill_popup_proxy_node &&
         g_autofill_popup_proxy_node->unique_id() == id;
}

bool BrowserAccessibilityManagerAndroid::Scroll(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint id,
    int direction) {
  BrowserAccessibilityAndroid* node = GetFromUniqueID(id);
  if (!node)
    return false;

  return node->Scroll(direction);
}

void BrowserAccessibilityManagerAndroid::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(
      tree, root_changed, changes);

  if (root_changed) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
    if (obj.is_null())
      return;

    Java_BrowserAccessibilityManager_handleNavigate(env, obj);
  }
}

bool
BrowserAccessibilityManagerAndroid::UseRootScrollOffsetsWhenComputingBounds() {
  // The Java layer handles the root scroll offset.
  return false;
}

BrowserAccessibilityAndroid*
BrowserAccessibilityManagerAndroid::GetFromUniqueID(int32_t unique_id) {
  return static_cast<BrowserAccessibilityAndroid*>(
      BrowserAccessibility::GetFromUniqueID(unique_id));
}

ScopedJavaLocalRef<jobject>
BrowserAccessibilityManagerAndroid::GetJavaRefFromRootManager() {
  BrowserAccessibilityManagerAndroid* root_manager =
      static_cast<BrowserAccessibilityManagerAndroid*>(
          GetRootManager());
  if (!root_manager)
    return ScopedJavaLocalRef<jobject>();

  JNIEnv* env = AttachCurrentThread();
  return root_manager->java_ref().get(env);
}

void BrowserAccessibilityManagerAndroid::CollectStats() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaRefFromRootManager();
  if (obj.is_null())
    return;

  int event_type_mask =
      Java_BrowserAccessibilityManager_getAccessibilityServiceEventTypeMask(
          env, obj);
  EVENT_TYPE_HISTOGRAM(event_type_mask, ANNOUNCEMENT);
  EVENT_TYPE_HISTOGRAM(event_type_mask, ASSIST_READING_CONTEXT);
  EVENT_TYPE_HISTOGRAM(event_type_mask, GESTURE_DETECTION_END);
  EVENT_TYPE_HISTOGRAM(event_type_mask, GESTURE_DETECTION_START);
  EVENT_TYPE_HISTOGRAM(event_type_mask, NOTIFICATION_STATE_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_EXPLORATION_GESTURE_END);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_EXPLORATION_GESTURE_START);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_INTERACTION_END);
  EVENT_TYPE_HISTOGRAM(event_type_mask, TOUCH_INTERACTION_START);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_ACCESSIBILITY_FOCUSED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_ACCESSIBILITY_FOCUS_CLEARED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_CLICKED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_CONTEXT_CLICKED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_FOCUSED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_HOVER_ENTER);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_HOVER_EXIT);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_LONG_CLICKED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_SCROLLED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_SELECTED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_TEXT_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, VIEW_TEXT_SELECTION_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask,
                       VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOWS_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOW_CONTENT_CHANGED);
  EVENT_TYPE_HISTOGRAM(event_type_mask, WINDOW_STATE_CHANGED);

  int feedback_type_mask =
      Java_BrowserAccessibilityManager_getAccessibilityServiceFeedbackTypeMask(
          env, obj);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, SPOKEN);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, HAPTIC);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, AUDIBLE);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, VISUAL);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, GENERIC);
  FEEDBACK_TYPE_HISTOGRAM(feedback_type_mask, BRAILLE);

  int flags_mask =
      Java_BrowserAccessibilityManager_getAccessibilityServiceFlagsMask(env,
                                                                        obj);
  FLAGS_HISTOGRAM(flags_mask, INCLUDE_NOT_IMPORTANT_VIEWS);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_TOUCH_EXPLORATION_MODE);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_ENHANCED_WEB_ACCESSIBILITY);
  FLAGS_HISTOGRAM(flags_mask, REPORT_VIEW_IDS);
  FLAGS_HISTOGRAM(flags_mask, REQUEST_FILTER_KEY_EVENTS);
  FLAGS_HISTOGRAM(flags_mask, RETRIEVE_INTERACTIVE_WINDOWS);
  FLAGS_HISTOGRAM(flags_mask, FORCE_DIRECT_BOOT_AWARE);

  int capabilities_mask =
      Java_BrowserAccessibilityManager_getAccessibilityServiceCapabilitiesMask(
          env, obj);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_RETRIEVE_WINDOW_CONTENT);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_TOUCH_EXPLORATION);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask,
                            CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_REQUEST_FILTER_KEY_EVENTS);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_CONTROL_MAGNIFICATION);
  CAPABILITY_TYPE_HISTOGRAM(capabilities_mask, CAN_PERFORM_GESTURES);
}

bool RegisterBrowserAccessibilityManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
