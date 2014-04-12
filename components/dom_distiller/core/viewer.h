// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_VIEWER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_VIEWER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace dom_distiller {

class DistilledArticleProto;
class DomDistillerServiceInterface;
class ViewerHandle;
class ViewRequestDelegate;

namespace viewer {

// Returns a full HTML page based on the given |article_proto|. This is supposed
// to displayed to the end user. The returned HTML should be considered unsafe,
// so callers must ensure rendering it does not compromise Chrome.
const std::string GetUnsafeHtml(const DistilledArticleProto* article_proto);

// Returns a full HTML page which displays a generic error.
const std::string GetErrorPageHtml();

// Returns the default CSS to be used for a viewer.
const std::string GetCss();

// Based on the given path, calls into the DomDistillerServiceInterface for
// viewing distilled content based on the |path|.
scoped_ptr<ViewerHandle> CreateViewRequest(
    DomDistillerServiceInterface* dom_distiller_service,
    const std::string& path,
    ViewRequestDelegate* view_request_delegate);

}  // namespace viewer

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_VIEWER_H_
