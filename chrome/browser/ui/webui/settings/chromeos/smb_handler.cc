// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/smb_handler.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/smb_client/smb_service.h"
#include "chrome/browser/profiles/profile.h"

namespace {
void DoNothingCallback(base::File::Error error) {
  return;
}
}  // namespace

namespace chromeos {
namespace settings {

SmbHandler::SmbHandler(Profile* profile) : profile_(profile) {}

SmbHandler::~SmbHandler() = default;

void SmbHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "smbMount",
      base::BindRepeating(&SmbHandler::HandleSmbMount, base::Unretained(this)));
}

void SmbHandler::HandleSmbMount(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string mountUrl;
  CHECK(args->GetString(0, &mountUrl));

  chromeos::smb_client::SmbService* const service =
      chromeos::smb_client::SmbService::Get(profile_);

  chromeos::file_system_provider::MountOptions mo;
  mo.display_name = mountUrl;
  mo.writable = true;

  service->Mount(mo, base::FilePath(mountUrl),
                 base::BindOnce(&DoNothingCallback));
}

}  // namespace settings
}  // namespace chromeos
