// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"

#include <utility>

#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

SafeDialDeviceDescriptionParser::SafeDialDeviceDescriptionParser() {}

SafeDialDeviceDescriptionParser::~SafeDialDeviceDescriptionParser() {}

void SafeDialDeviceDescriptionParser::Start(
    const std::string& xml_text,
    const DeviceDescriptionCallback& callback) {
  DVLOG(2) << "Start parsing device description...";
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(callback);

  if (!utility_process_mojo_client_) {
    DVLOG(2) << "Start utility process in background...";
    utility_process_mojo_client_ =
        base::MakeUnique<content::UtilityProcessMojoClient<
            chrome::mojom::DialDeviceDescriptionParser>>(
            l10n_util::GetStringUTF16(
                IDS_UTILITY_PROCESS_DIAL_DEVICE_DESCRIPTION_PARSER_NAME));

    utility_process_mojo_client_->set_error_callback(
        base::Bind(callback, nullptr,
                   chrome::mojom::DialParsingError::UTILITY_PROCESS_ERROR));

    // This starts utility process in the background.
    utility_process_mojo_client_->Start();
  }

  // This call is queued up until the Mojo message pipe has been established to
  // the service running in the utility process.
  utility_process_mojo_client_->service()->ParseDialDeviceDescription(xml_text,
                                                                      callback);
}

}  // namespace media_router
