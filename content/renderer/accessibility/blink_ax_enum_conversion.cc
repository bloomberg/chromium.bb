// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/blink_ax_enum_conversion.h"

#include "base/logging.h"

namespace content {

void AXStateFromBlink(const blink::WebAXObject& o, ui::AXNodeData* dst) {
  blink::WebAXExpanded expanded = o.IsExpanded();
  if (expanded) {
    if (expanded == blink::kWebAXExpandedCollapsed)
      dst->AddState(ax::mojom::State::kCollapsed);
    else if (expanded == blink::kWebAXExpandedExpanded)
      dst->AddState(ax::mojom::State::kExpanded);
  }

  if (o.CanSetFocusAttribute())
    dst->AddState(ax::mojom::State::kFocusable);

  if (o.HasPopup())
    dst->SetHasPopup(AXHasPopupFromBlink(o.HasPopup()));
  else if (o.Role() == ax::mojom::Role::kPopUpButton)
    dst->SetHasPopup(ax::mojom::HasPopup::kMenu);

  if (o.IsAutofillAvailable())
    dst->AddState(ax::mojom::State::kAutofillAvailable);

  if (o.IsHovered())
    dst->AddState(ax::mojom::State::kHovered);

  if (!o.IsVisible())
    dst->AddState(ax::mojom::State::kInvisible);

  if (o.IsLinked())
    dst->AddState(ax::mojom::State::kLinked);

  if (o.IsMultiline())
    dst->AddState(ax::mojom::State::kMultiline);

  if (o.IsMultiSelectable())
    dst->AddState(ax::mojom::State::kMultiselectable);

  if (o.IsPasswordField())
    dst->AddState(ax::mojom::State::kProtected);

  if (o.IsRequired())
    dst->AddState(ax::mojom::State::kRequired);

  if (o.IsEditable())
    dst->AddState(ax::mojom::State::kEditable);

  if (o.IsSelected() != blink::kWebAXSelectedStateUndefined) {
    dst->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                          o.IsSelected() == blink::kWebAXSelectedStateTrue);
  }

  if (o.IsRichlyEditable())
    dst->AddState(ax::mojom::State::kRichlyEditable);

  if (o.IsVisited())
    dst->AddState(ax::mojom::State::kVisited);

  if (o.Orientation() == blink::kWebAXOrientationVertical)
    dst->AddState(ax::mojom::State::kVertical);
  else if (o.Orientation() == blink::kWebAXOrientationHorizontal)
    dst->AddState(ax::mojom::State::kHorizontal);

  if (o.IsVisited())
    dst->AddState(ax::mojom::State::kVisited);
}

ax::mojom::DefaultActionVerb AXDefaultActionVerbFromBlink(
    blink::WebAXDefaultActionVerb action_verb) {
  switch (action_verb) {
    case blink::WebAXDefaultActionVerb::kNone:
      return ax::mojom::DefaultActionVerb::kNone;
    case blink::WebAXDefaultActionVerb::kActivate:
      return ax::mojom::DefaultActionVerb::kActivate;
    case blink::WebAXDefaultActionVerb::kCheck:
      return ax::mojom::DefaultActionVerb::kCheck;
    case blink::WebAXDefaultActionVerb::kClick:
      return ax::mojom::DefaultActionVerb::kClick;
    case blink::WebAXDefaultActionVerb::kClickAncestor:
      return ax::mojom::DefaultActionVerb::kClickAncestor;
    case blink::WebAXDefaultActionVerb::kJump:
      return ax::mojom::DefaultActionVerb::kJump;
    case blink::WebAXDefaultActionVerb::kOpen:
      return ax::mojom::DefaultActionVerb::kOpen;
    case blink::WebAXDefaultActionVerb::kPress:
      return ax::mojom::DefaultActionVerb::kPress;
    case blink::WebAXDefaultActionVerb::kSelect:
      return ax::mojom::DefaultActionVerb::kSelect;
    case blink::WebAXDefaultActionVerb::kUncheck:
      return ax::mojom::DefaultActionVerb::kUncheck;
  }
  NOTREACHED();
  return ax::mojom::DefaultActionVerb::kNone;
}

ax::mojom::MarkerType AXMarkerTypeFromBlink(
    blink::WebAXMarkerType marker_type) {
  switch (marker_type) {
    case blink::kWebAXMarkerTypeSpelling:
      return ax::mojom::MarkerType::kSpelling;
    case blink::kWebAXMarkerTypeGrammar:
      return ax::mojom::MarkerType::kGrammar;
    case blink::kWebAXMarkerTypeTextMatch:
      return ax::mojom::MarkerType::kTextMatch;
    case blink::kWebAXMarkerTypeActiveSuggestion:
      return ax::mojom::MarkerType::kActiveSuggestion;
    case blink::kWebAXMarkerTypeSuggestion:
      return ax::mojom::MarkerType::kSuggestion;
  }
  NOTREACHED();
  return ax::mojom::MarkerType::kNone;
}

ax::mojom::TextDirection AXTextDirectionFromBlink(
    blink::WebAXTextDirection text_direction) {
  switch (text_direction) {
    case blink::kWebAXTextDirectionLR:
      return ax::mojom::TextDirection::kLtr;
    case blink::kWebAXTextDirectionRL:
      return ax::mojom::TextDirection::kRtl;
    case blink::kWebAXTextDirectionTB:
      return ax::mojom::TextDirection::kTtb;
    case blink::kWebAXTextDirectionBT:
      return ax::mojom::TextDirection::kBtt;
  }
  NOTREACHED();
  return ax::mojom::TextDirection::kNone;
}

ax::mojom::TextPosition AXTextPositionFromBlink(
    blink::WebAXTextPosition text_position) {
  switch (text_position) {
    case blink::kWebAXTextPositionNone:
      return ax::mojom::TextPosition::kNone;
    case blink::kWebAXTextPositionSubscript:
      return ax::mojom::TextPosition::kSubscript;
    case blink::kWebAXTextPositionSuperscript:
      return ax::mojom::TextPosition::kSuperscript;
  }
  NOTREACHED();
  return ax::mojom::TextPosition::kNone;
}

ax::mojom::TextStyle AXTextStyleFromBlink(blink::WebAXTextStyle text_style) {
  uint32_t browser_text_style =
      static_cast<uint32_t>(ax::mojom::TextStyle::kNone);
  if (text_style & blink::kWebAXTextStyleBold)
    browser_text_style |=
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleBold);
  if (text_style & blink::kWebAXTextStyleItalic)
    browser_text_style |=
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleItalic);
  if (text_style & blink::kWebAXTextStyleUnderline)
    browser_text_style |=
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleUnderline);
  if (text_style & blink::kWebAXTextStyleLineThrough)
    browser_text_style |=
        static_cast<int32_t>(ax::mojom::TextStyle::kTextStyleLineThrough);
  return static_cast<ax::mojom::TextStyle>(browser_text_style);
}

ax::mojom::AriaCurrentState AXAriaCurrentStateFromBlink(
    blink::WebAXAriaCurrentState aria_current_state) {
  switch (aria_current_state) {
    case blink::kWebAXAriaCurrentStateUndefined:
      return ax::mojom::AriaCurrentState::kNone;
    case blink::kWebAXAriaCurrentStateFalse:
      return ax::mojom::AriaCurrentState::kFalse;
    case blink::kWebAXAriaCurrentStateTrue:
      return ax::mojom::AriaCurrentState::kTrue;
    case blink::kWebAXAriaCurrentStatePage:
      return ax::mojom::AriaCurrentState::kPage;
    case blink::kWebAXAriaCurrentStateStep:
      return ax::mojom::AriaCurrentState::kStep;
    case blink::kWebAXAriaCurrentStateLocation:
      return ax::mojom::AriaCurrentState::kLocation;
    case blink::kWebAXAriaCurrentStateDate:
      return ax::mojom::AriaCurrentState::kDate;
    case blink::kWebAXAriaCurrentStateTime:
      return ax::mojom::AriaCurrentState::kTime;
  }

  NOTREACHED();
  return ax::mojom::AriaCurrentState::kNone;
}

ax::mojom::HasPopup AXHasPopupFromBlink(blink::WebAXHasPopup has_popup) {
  switch (has_popup) {
    case blink::kWebAXHasPopupFalse:
      return ax::mojom::HasPopup::kFalse;
    case blink::kWebAXHasPopupTrue:
      return ax::mojom::HasPopup::kTrue;
    case blink::kWebAXHasPopupMenu:
      return ax::mojom::HasPopup::kMenu;
    case blink::kWebAXHasPopupListbox:
      return ax::mojom::HasPopup::kListbox;
    case blink::kWebAXHasPopupTree:
      return ax::mojom::HasPopup::kTree;
    case blink::kWebAXHasPopupGrid:
      return ax::mojom::HasPopup::kGrid;
    case blink::kWebAXHasPopupDialog:
      return ax::mojom::HasPopup::kDialog;
  }

  NOTREACHED();
  return ax::mojom::HasPopup::kFalse;
}

ax::mojom::InvalidState AXInvalidStateFromBlink(
    blink::WebAXInvalidState invalid_state) {
  switch (invalid_state) {
    case blink::kWebAXInvalidStateUndefined:
      return ax::mojom::InvalidState::kNone;
    case blink::kWebAXInvalidStateFalse:
      return ax::mojom::InvalidState::kFalse;
    case blink::kWebAXInvalidStateTrue:
      return ax::mojom::InvalidState::kTrue;
    case blink::kWebAXInvalidStateSpelling:
      return ax::mojom::InvalidState::kSpelling;
    case blink::kWebAXInvalidStateGrammar:
      return ax::mojom::InvalidState::kGrammar;
    case blink::kWebAXInvalidStateOther:
      return ax::mojom::InvalidState::kOther;
  }
  NOTREACHED();
  return ax::mojom::InvalidState::kNone;
}

ax::mojom::CheckedState AXCheckedStateFromBlink(
    blink::WebAXCheckedState checked_state) {
  switch (checked_state) {
    case blink::kWebAXCheckedUndefined:
      return ax::mojom::CheckedState::kNone;
    case blink::kWebAXCheckedTrue:
      return ax::mojom::CheckedState::kTrue;
    case blink::kWebAXCheckedMixed:
      return ax::mojom::CheckedState::kMixed;
    case blink::kWebAXCheckedFalse:
      return ax::mojom::CheckedState::kFalse;
  }
  NOTREACHED();
  return ax::mojom::CheckedState::kNone;
}

ax::mojom::SortDirection AXSortDirectionFromBlink(
    blink::WebAXSortDirection sort_direction) {
  switch (sort_direction) {
    case blink::kWebAXSortDirectionUndefined:
      return ax::mojom::SortDirection::kNone;
    case blink::kWebAXSortDirectionNone:
      return ax::mojom::SortDirection::kUnsorted;
    case blink::kWebAXSortDirectionAscending:
      return ax::mojom::SortDirection::kAscending;
    case blink::kWebAXSortDirectionDescending:
      return ax::mojom::SortDirection::kDescending;
    case blink::kWebAXSortDirectionOther:
      return ax::mojom::SortDirection::kOther;
  }
  NOTREACHED();
  return ax::mojom::SortDirection::kNone;
}

ax::mojom::NameFrom AXNameFromFromBlink(blink::WebAXNameFrom name_from) {
  switch (name_from) {
    case blink::kWebAXNameFromUninitialized:
      return ax::mojom::NameFrom::kUninitialized;
    case blink::kWebAXNameFromAttribute:
      return ax::mojom::NameFrom::kAttribute;
    case blink::kWebAXNameFromAttributeExplicitlyEmpty:
      return ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
    case blink::kWebAXNameFromCaption:
      return ax::mojom::NameFrom::kRelatedElement;
    case blink::kWebAXNameFromContents:
      return ax::mojom::NameFrom::kContents;
    case blink::kWebAXNameFromPlaceholder:
      return ax::mojom::NameFrom::kPlaceholder;
    case blink::kWebAXNameFromRelatedElement:
      return ax::mojom::NameFrom::kRelatedElement;
    case blink::kWebAXNameFromValue:
      return ax::mojom::NameFrom::kValue;
    case blink::kWebAXNameFromTitle:
      return ax::mojom::NameFrom::kAttribute;
  }
  NOTREACHED();
  return ax::mojom::NameFrom::kNone;
}

ax::mojom::DescriptionFrom AXDescriptionFromFromBlink(
    blink::WebAXDescriptionFrom description_from) {
  switch (description_from) {
    case blink::kWebAXDescriptionFromUninitialized:
      return ax::mojom::DescriptionFrom::kUninitialized;
    case blink::kWebAXDescriptionFromAttribute:
      return ax::mojom::DescriptionFrom::kAttribute;
    case blink::kWebAXDescriptionFromContents:
      return ax::mojom::DescriptionFrom::kContents;
    case blink::kWebAXDescriptionFromRelatedElement:
      return ax::mojom::DescriptionFrom::kRelatedElement;
  }
  NOTREACHED();
  return ax::mojom::DescriptionFrom::kNone;
}

ax::mojom::TextAffinity AXTextAffinityFromBlink(
    blink::WebAXTextAffinity affinity) {
  switch (affinity) {
    case blink::kWebAXTextAffinityUpstream:
      return ax::mojom::TextAffinity::kUpstream;
    case blink::kWebAXTextAffinityDownstream:
      return ax::mojom::TextAffinity::kDownstream;
  }
  NOTREACHED();
  return ax::mojom::TextAffinity::kDownstream;
}

}  // namespace content.
