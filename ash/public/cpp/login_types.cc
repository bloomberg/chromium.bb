// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_types.h"

namespace ash {

EasyUnlockIconOptions::EasyUnlockIconOptions() = default;
EasyUnlockIconOptions::EasyUnlockIconOptions(
    const EasyUnlockIconOptions& other) = default;
EasyUnlockIconOptions::EasyUnlockIconOptions(EasyUnlockIconOptions&& other) =
    default;
EasyUnlockIconOptions::~EasyUnlockIconOptions() = default;

EasyUnlockIconOptions& EasyUnlockIconOptions::operator=(
    const EasyUnlockIconOptions& other) = default;
EasyUnlockIconOptions& EasyUnlockIconOptions::operator=(
    EasyUnlockIconOptions&& other) = default;

InputMethodItem::InputMethodItem() = default;
InputMethodItem::InputMethodItem(const InputMethodItem& other) = default;
InputMethodItem::InputMethodItem(InputMethodItem&& other) = default;
InputMethodItem::~InputMethodItem() = default;

InputMethodItem& InputMethodItem::operator=(const InputMethodItem& other) =
    default;
InputMethodItem& InputMethodItem::operator=(InputMethodItem&& other) = default;

LocaleItem::LocaleItem() = default;
LocaleItem::LocaleItem(const LocaleItem& other) = default;
LocaleItem::LocaleItem(LocaleItem&& other) = default;
LocaleItem::~LocaleItem() = default;

LocaleItem& LocaleItem::operator=(const LocaleItem& other) = default;
LocaleItem& LocaleItem::operator=(LocaleItem&& other) = default;

PublicAccountInfo::PublicAccountInfo() = default;
PublicAccountInfo::PublicAccountInfo(const PublicAccountInfo& other) = default;
PublicAccountInfo::PublicAccountInfo(PublicAccountInfo&& other) = default;
PublicAccountInfo::~PublicAccountInfo() = default;

PublicAccountInfo& PublicAccountInfo::operator=(
    const PublicAccountInfo& other) = default;
PublicAccountInfo& PublicAccountInfo::operator=(PublicAccountInfo&& other) =
    default;

LoginUserInfo::LoginUserInfo() = default;
LoginUserInfo::LoginUserInfo(const LoginUserInfo& other) = default;
LoginUserInfo::LoginUserInfo(LoginUserInfo&& other) = default;
LoginUserInfo::~LoginUserInfo() = default;

LoginUserInfo& LoginUserInfo::operator=(const LoginUserInfo& other) = default;
LoginUserInfo& LoginUserInfo::operator=(LoginUserInfo&& other) = default;

}  // namespace ash
