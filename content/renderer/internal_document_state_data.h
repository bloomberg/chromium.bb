// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
#define CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_

#include "base/supports_user_data.h"

namespace WebKit {
class WebDataSource;
}

namespace content {

// Stores internal state per WebDataSource.
class InternalDocumentStateData : public base::SupportsUserData::Data {
 public:
  InternalDocumentStateData();

  static InternalDocumentStateData* FromDataSource(WebKit::WebDataSource* ds);

  // Set to true once RenderViewImpl::didFirstVisuallyNonEmptyLayout() is
  // invoked.
  bool did_first_visually_non_empty_layout() const {
    return did_first_visually_non_empty_layout_;
  }
  void set_did_first_visually_non_empty_layout(bool value) {
    did_first_visually_non_empty_layout_ = value;
  }

  // Set to true once RenderViewImpl::DidFlushPaint() is inovked after
  // RenderViewImpl::didFirstVisuallyNonEmptyLayout(). In other words after the
  // page has painted something.
  bool did_first_visually_non_empty_paint() const {
    return did_first_visually_non_empty_paint_;
  }
  void set_did_first_visually_non_empty_paint(bool value) {
    did_first_visually_non_empty_paint_ = value;
  }

 protected:
  virtual ~InternalDocumentStateData();

 private:
  bool did_first_visually_non_empty_layout_;
  bool did_first_visually_non_empty_paint_;

  DISALLOW_COPY_AND_ASSIGN(InternalDocumentStateData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INTERNAL_DOCUMENT_STATE_DATA_H_
