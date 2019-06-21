// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/nfc_host.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "content/public/android/content_jni_headers/NfcHost_jni.h"
#include "content/public/browser/system_connector.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/nfc.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

base::AtomicSequenceNumber g_unique_id;

}  // namespace

NFCHost::NFCHost(WebContents* web_contents)
    : id_(g_unique_id.GetNext()), web_contents_(web_contents) {
  DCHECK(web_contents_);

  JNIEnv* env = base::android::AttachCurrentThread();

  java_nfc_host_.Reset(
      Java_NfcHost_create(env, web_contents_->GetJavaWebContents(), id_));

  service_manager::Connector* connector = content::GetSystemConnector();
  if (connector) {
    connector->BindInterface(device::mojom::kServiceName,
                             mojo::MakeRequest(&nfc_provider_));
  }
}

void NFCHost::GetNFC(device::mojom::NFCRequest request) {
  // Connect to an NFC object, associating it with |id_|.
  nfc_provider_->GetNFCForHost(id_, std::move(request));
}

NFCHost::~NFCHost() {}

}  // namespace content
