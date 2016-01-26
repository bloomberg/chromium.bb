// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_ipc_host_impl.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_bridge_service.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

namespace arc {
namespace {

ui::TextInputType ConvertTextInputType(arc::TextInputType ipc_type) {
  // The two enum types are similar, but intentionally made not identical.
  // We cannot force them to be in sync. If we do, updates in ui::TextInputType
  // must always be propagated to the arc::TextInputType mojo definition in
  // ARC container side, which is in a different repository than Chromium.
  // We don't want such dependency.
  //
  // That's why we need a lengthy switch statement instead of static_cast
  // guarded by a static assert on the two enums to be in sync.
  switch (ipc_type) {
  case arc::TextInputType::NONE:
    return ui::TEXT_INPUT_TYPE_NONE;
  case arc::TextInputType::TEXT:
    return ui::TEXT_INPUT_TYPE_TEXT;
  case arc::TextInputType::PASSWORD:
    return ui::TEXT_INPUT_TYPE_PASSWORD;
  case arc::TextInputType::SEARCH:
    return ui::TEXT_INPUT_TYPE_SEARCH;
  case arc::TextInputType::EMAIL:
    return ui::TEXT_INPUT_TYPE_EMAIL;
  case arc::TextInputType::NUMBER:
    return ui::TEXT_INPUT_TYPE_NUMBER;
  case arc::TextInputType::TELEPHONE:
    return ui::TEXT_INPUT_TYPE_TELEPHONE;
  case arc::TextInputType::URL:
    return ui::TEXT_INPUT_TYPE_URL;
  case arc::TextInputType::DATE:
    return ui::TEXT_INPUT_TYPE_DATE;
  case arc::TextInputType::TIME:
    return ui::TEXT_INPUT_TYPE_TIME;
  case arc::TextInputType::DATETIME:
    return ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL;
  default:
    return ui::TEXT_INPUT_TYPE_TEXT;
  }
}

mojo::Array<arc::CompositionSegmentPtr> ConvertSegments(
    const ui::CompositionText& composition) {
  mojo::Array<arc::CompositionSegmentPtr> segments =
      mojo::Array<arc::CompositionSegmentPtr>::New(0);
  for (const ui::CompositionUnderline& underline : composition.underlines) {
    arc::CompositionSegmentPtr segment = arc::CompositionSegment::New();
    segment->start_offset = underline.start_offset;
    segment->end_offset = underline.end_offset;
    segment->emphasized = (underline.thick ||
        (composition.selection.start() == underline.start_offset &&
         composition.selection.end() == underline.end_offset));
    segments.push_back(std::move(segment));
  }
  return segments;
}

}  // namespace

ArcImeIpcHostImpl::ArcImeIpcHostImpl(Delegate* delegate,
                                     ArcBridgeService* bridge_service)
    : binding_(this), delegate_(delegate), bridge_service_(bridge_service) {
  bridge_service_->AddObserver(this);
}

ArcImeIpcHostImpl::~ArcImeIpcHostImpl() {
  bridge_service_->RemoveObserver(this);
}

void ArcImeIpcHostImpl::OnImeInstanceReady() {
  bridge_service_->ime_instance()->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcImeIpcHostImpl::SendSetCompositionText(
    const ui::CompositionText& composition) {
  ImeInstance* ime_instance = bridge_service_->ime_instance();
  if (!ime_instance) {
    LOG(ERROR) << "ArcImeInstance method called before being ready.";
    return;
  }

  ime_instance->SetCompositionText(base::UTF16ToUTF8(composition.text),
                                   ConvertSegments(composition));
}

void ArcImeIpcHostImpl::SendConfirmCompositionText() {
  ImeInstance* ime_instance = bridge_service_->ime_instance();
  if (!ime_instance) {
    LOG(ERROR) << "ArcImeInstance method called before being ready.";
    return;
  }

  ime_instance->ConfirmCompositionText();
}

void ArcImeIpcHostImpl::SendInsertText(const base::string16& text) {
  ImeInstance* ime_instance = bridge_service_->ime_instance();
  if (!ime_instance) {
    LOG(ERROR) << "ArcImeInstance method called before being ready.";
    return;
  }

  ime_instance->InsertText(base::UTF16ToUTF8(text));
}

void ArcImeIpcHostImpl::OnTextInputTypeChanged(arc::TextInputType type) {
  delegate_->OnTextInputTypeChanged(ConvertTextInputType(type));
}

void ArcImeIpcHostImpl::OnCursorRectChanged(arc::CursorRectPtr rect) {
  delegate_->OnCursorRectChanged(gfx::Rect(
      rect->left,
      rect->top,
      rect->right - rect->left,
      rect->bottom - rect->top));
}

}  // namespace arc
