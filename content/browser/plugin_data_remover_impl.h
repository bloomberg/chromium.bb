// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PLUGIN_DATA_REMOVER_IMPL_H_
#define CONTENT_BROWSER_PLUGIN_DATA_REMOVER_IMPL_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/plugin_data_remover.h"

class CONTENT_EXPORT PluginDataRemoverImpl : public content::PluginDataRemover {
 public:
  explicit PluginDataRemoverImpl(
      const content::ResourceContext& resource_context);
  virtual ~PluginDataRemoverImpl();

  // content::PluginDataRemover implementation:
  virtual base::WaitableEvent* StartRemoving(base::Time begin_time) OVERRIDE;

  // The plug-in whose data should be removed (usually Flash) is specified via
  // its MIME type. This method sets a different MIME type in order to call a
  // different plug-in (for example in tests).
  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

 private:
  class Context;

  std::string mime_type_;
  // The resource context for the profile.
  const content::ResourceContext& resource_context_;

  // This allows this object to be deleted on the UI thread while it's still
  // being used on the IO thread.
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(PluginDataRemoverImpl);
};

#endif  // CONTENT_BROWSER_PLUGIN_DATA_REMOVER_IMPL_H_
