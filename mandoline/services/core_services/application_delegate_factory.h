// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_FACTORY_H_
#define MANDOLINE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace mojo {
class ApplicationDelegate;
}

namespace core_services {

// This is always called to create a platform specific ApplicationDelegate.
scoped_ptr<mojo::ApplicationDelegate> CreatePlatformSpecificApplicationDelegate(
    const std::string& url);

// The following are conditionally called. You do not need to call these from
// CreatePlatformSpecificApplicationDelegate(), they are called (if appropriate)
// before CreatePlatformSpecificApplicationDelegate().
#if !defined(OS_ANDROID)
scoped_ptr<mojo::ApplicationDelegate> CreateApplicationDelegateNotAndroid(
    const std::string& url);
#endif

}  // namespace core_services

#endif  // MANDOLINE_SERVICES_CORE_SERVICES_APPLICATION_DELEGATE_FACTORY_H_
