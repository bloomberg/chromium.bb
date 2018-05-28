// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_accessory_controller.h"

#include <vector>

#include "chrome/browser/password_manager/password_accessory_view_interface.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::PasswordForm;
using Item = PasswordAccessoryViewInterface::AccessoryItem;

// static
base::WeakPtr<PasswordAccessoryController>
PasswordAccessoryController::GetOrCreate(
    base::WeakPtr<PasswordAccessoryController> previous,
    gfx::NativeView container_view) {
  // TODO(fhorschig): Check if the container_view really must be different.
  if (previous.get() && previous->container_view() == container_view)
    return previous;

  std::unique_ptr<PasswordAccessoryController> controller(
      new PasswordAccessoryController(container_view));
  auto weak_controller = controller->weak_ptr_factory_.GetWeakPtr();
  // TODO(fhorschig): Move ownership to view.
  return weak_controller;  // == NULL for now.
}

// static
std::unique_ptr<PasswordAccessoryController>
PasswordAccessoryController::CreateForTesting(
    std::unique_ptr<PasswordAccessoryViewInterface> view) {
  std::unique_ptr<PasswordAccessoryController> controller(
      new PasswordAccessoryController(/*container_view=*/nullptr));
  controller->view_ = std::move(view);
  return controller;
}

PasswordAccessoryController::PasswordAccessoryController(
    gfx::NativeView container_view)
    : container_view_(container_view), weak_ptr_factory_(this) {}

PasswordAccessoryController::~PasswordAccessoryController() = default;

void PasswordAccessoryController::Destroy() {
  // The view should not exist without controller. Also, destroying the view
  // should destroy the controller.
  view_.reset();
}

void PasswordAccessoryController::OnPasswordsAvailable(
    const std::map<base::string16, const PasswordForm*>& best_matches,
    const GURL& origin) {
  DCHECK(view_);
  std::vector<Item> items;
  base::string16 passwords_title_str = l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_TITLE);
  items.emplace_back(passwords_title_str, passwords_title_str,
                     /*is_password=*/false, Item::Type::LABEL);
  if (best_matches.empty()) {
    base::string16 passwords_empty_str = l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_LIST_EMPTY_MESSAGE);
    items.emplace_back(passwords_empty_str, passwords_empty_str,
                       /*is_password=*/false, Item::Type::LABEL);
  }
  for (const auto& pair : best_matches) {
    const PasswordForm* form = pair.second;
    base::string16 username = GetDisplayUsername(*form);
    items.emplace_back(username, username,
                       /*is_password=*/false, Item::Type::SUGGESTION);
    items.emplace_back(
        form->password_value,
        l10n_util::GetStringFUTF16(
            IDS_PASSWORD_MANAGER_ACCESSORY_PASSWORD_DESCRIPTION, username),
        /*is_password=*/true, Item::Type::SUGGESTION);
  }
  view_->OnItemsAvailable(origin, items);
}

gfx::NativeView PasswordAccessoryController::container_view() {
  return container_view_;
}
