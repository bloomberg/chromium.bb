// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeWebCompositorSoftwareOutputDevice_h
#define FakeWebCompositorSoftwareOutputDevice_h

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkDevice.h"
#include <public/WebCompositorSoftwareOutputDevice.h>
#include <public/WebImage.h>
#include <public/WebSize.h>

namespace WebKit {

class FakeWebCompositorSoftwareOutputDevice : public WebCompositorSoftwareOutputDevice {
public:
    virtual WebImage* lock(bool forWrite) OVERRIDE
    {
        ASSERT(m_device.get());
        m_image = m_device->accessBitmap(forWrite);
        return &m_image;
    }
    virtual void unlock() OVERRIDE
    {
        m_image.reset();
    }

    virtual void didChangeViewportSize(WebSize size) OVERRIDE
    {
        if (m_device.get() && size.width == m_device->width() && size.height == m_device->height())
            return;

        m_device.reset(new SkDevice(SkBitmap::kARGB_8888_Config, size.width, size.height, true));
    }

private:
    scoped_ptr<SkDevice> m_device;
    WebImage m_image;
};

} // namespace WebKit

#endif // FakeWebCompositorSoftwareOutputDevice_h
