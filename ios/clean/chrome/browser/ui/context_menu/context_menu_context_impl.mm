// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/context_menu/context_menu_context_impl.h"

#include "base/strings/utf_string_conversions.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContextMenuContextImpl () {
  // The parameters passed on initialization.
  web::ContextMenuParams _params;
  // The context menu link's script, if of the javascript: scheme.
  base::string16 _script;
}

@end

@implementation ContextMenuContextImpl

- (instancetype)initWithParams:(const web::ContextMenuParams&)params {
  if ((self = [super init])) {
    _params = params;
    const GURL& linkURL = _params.link_url;
    if (linkURL.is_valid() && linkURL.SchemeIs(url::kJavaScriptScheme))
      _script = base::UTF8ToUTF16(linkURL.GetContent());
  }
  return self;
}

#pragma mark - Accessors

- (NSString*)menuTitle {
  return _params.menu_title.get();
}

- (const GURL&)linkURL {
  return _params.link_url;
}

- (const GURL&)imageURL {
  return _params.src_url;
}

- (const base::string16&)script {
  return _script;
}

@end
