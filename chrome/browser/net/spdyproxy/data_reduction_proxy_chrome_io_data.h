// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_IO_DATA_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_IO_DATA_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class DataReductionProxyChromeConfigurator;
class PrefService;

namespace base {
class SingleThreadTaskRunner;
}

namespace data_reduction_proxy {
class DataReductionProxyIOData;
}

namespace net {
class NetLog;
}

// Constructs DataReductionProxyIOData suitable for use by ProfileImpl and
// ProfileImplIOData.
scoped_ptr<data_reduction_proxy::DataReductionProxyIOData>
CreateDataReductionProxyChromeIOData(
    net::NetLog* net_log,
    PrefService* prefs,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread_runner,
    bool enable_quic);

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_IO_DATA_H_
