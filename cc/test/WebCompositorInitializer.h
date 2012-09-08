// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositorInitializer_h
#define WebCompositorInitializer_h

#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <wtf/Noncopyable.h>

namespace WebKit {
class WebThread;
}

namespace WebKitTests {

class WebCompositorInitializer {
    WTF_MAKE_NONCOPYABLE(WebCompositorInitializer);
public:
    explicit WebCompositorInitializer(WebKit::WebThread* thread)
    {
        WebKit::Platform::current()->compositorSupport()->initialize(thread);
    }

    ~WebCompositorInitializer()
    {
        WebKit::Platform::current()->compositorSupport()->shutdown();
    }
};

}

#endif // WebCompositorInitializer_h
