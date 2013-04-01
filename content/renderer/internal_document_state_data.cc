// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/internal_document_state_data.h"

#include "content/public/renderer/document_state.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"

namespace content {

namespace {

// Key InternalDocumentStateData is stored under in DocumentState.
const char kUserDataKey[] = "InternalDocumentStateData";

}

InternalDocumentStateData::InternalDocumentStateData()
    : did_first_visually_non_empty_layout_(false),
      did_first_visually_non_empty_paint_(false) {
}

// static
InternalDocumentStateData* InternalDocumentStateData::FromDataSource(
    WebKit::WebDataSource* ds) {
  DocumentState* document_state = static_cast<DocumentState*>(ds->extraData());
  DCHECK(document_state);
  InternalDocumentStateData* data = static_cast<InternalDocumentStateData*>(
      document_state->GetUserData(&kUserDataKey));
  if (!data) {
    data = new InternalDocumentStateData;
    document_state->SetUserData(&kUserDataKey, data);
  }
  return data;
}

InternalDocumentStateData::~InternalDocumentStateData() {
}

}  // namespace content
