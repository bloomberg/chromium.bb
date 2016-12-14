// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "extensions/browser/extension_registry.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

ExtensionMessageBubbleBridge::ExtensionMessageBubbleBridge(
    std::unique_ptr<extensions::ExtensionMessageBubbleController> controller)
    : controller_(std::move(controller)) {}

ExtensionMessageBubbleBridge::~ExtensionMessageBubbleBridge() {}

bool ExtensionMessageBubbleBridge::ShouldShow() {
  return controller_->ShouldShow();
}

bool ExtensionMessageBubbleBridge::ShouldCloseOnDeactivate() {
  return controller_->CloseOnDeactivate();
}

bool ExtensionMessageBubbleBridge::IsPolicyIndicationNeeded(
    const extensions::Extension* extension) {
  return controller_->delegate()->SupportsPolicyIndicator() &&
         extensions::Manifest::IsPolicyLocation(extension->location());
}

base::string16 ExtensionMessageBubbleBridge::GetHeadingText() {
  return controller_->delegate()->GetTitle();
}

base::string16 ExtensionMessageBubbleBridge::GetBodyText(
    bool anchored_to_action) {
  return controller_->delegate()->GetMessageBody(
      anchored_to_action, controller_->GetExtensionIdList().size());
}

base::string16 ExtensionMessageBubbleBridge::GetItemListText() {
  return controller_->GetExtensionListForDisplay();
}

base::string16 ExtensionMessageBubbleBridge::GetActionButtonText() {
  const extensions::ExtensionIdList& list = controller_->GetExtensionIdList();
  DCHECK(!list.empty());
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(controller_->profile())
          ->enabled_extensions()
          .GetByID(list[0]);

  DCHECK(extension);
  // An empty string is returned so that we don't display the button prompting
  // to remove policy-installed extensions.
  if (IsPolicyIndicationNeeded(extension))
    return base::string16();
  return controller_->delegate()->GetActionButtonLabel();
}

base::string16 ExtensionMessageBubbleBridge::GetDismissButtonText() {
  return controller_->delegate()->GetDismissButtonLabel();
}

base::string16 ExtensionMessageBubbleBridge::GetLearnMoreButtonText() {
  return controller_->delegate()->GetLearnMoreLabel();
}

std::string ExtensionMessageBubbleBridge::GetAnchorActionId() {
  return controller_->GetExtensionIdList().size() == 1u
             ? controller_->GetExtensionIdList()[0]
             : std::string();
}

void ExtensionMessageBubbleBridge::OnBubbleShown() {
  controller_->OnShown();
}

void ExtensionMessageBubbleBridge::OnBubbleClosed(CloseAction action) {
  switch (action) {
    case CLOSE_DISMISS_USER_ACTION:
    case CLOSE_DISMISS_DEACTIVATION: {
      bool close_by_deactivate = action == CLOSE_DISMISS_DEACTIVATION;
      controller_->OnBubbleDismiss(close_by_deactivate);
      break;
    }
    case CLOSE_EXECUTE:
      controller_->OnBubbleAction();
      break;
    case CLOSE_LEARN_MORE:
      controller_->OnLinkClicked();
      break;
  }
}

std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
ExtensionMessageBubbleBridge::GetExtraViewInfo() {
  const extensions::ExtensionIdList& list = controller_->GetExtensionIdList();
  int include_mask = controller_->delegate()->ShouldLimitToEnabledExtensions() ?
      extensions::ExtensionRegistry::ENABLED :
      extensions::ExtensionRegistry::EVERYTHING;
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(controller_->profile())
          ->GetExtensionById(list[0], include_mask);

  DCHECK(extension);

  std::unique_ptr<ExtraViewInfo> extra_view_info =
      base::MakeUnique<ExtraViewInfo>();

  if (IsPolicyIndicationNeeded(extension)) {
    DCHECK_EQ(1u, list.size());
    extra_view_info->resource_id = gfx::VectorIconId::BUSINESS;
    extra_view_info->text =
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN);
    extra_view_info->is_text_linked = false;
  } else {
    extra_view_info->text = controller_->delegate()->GetLearnMoreLabel();
    extra_view_info->is_text_linked = true;
  }

  return extra_view_info;
}
