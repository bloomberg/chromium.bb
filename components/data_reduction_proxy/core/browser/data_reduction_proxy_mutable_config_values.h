// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace data_reduction_proxy {

class DataReductionProxyParams;

// A |DataReductionProxyConfigValues| which is permitted to change its
// underlying values via the UpdateValues method.
class DataReductionProxyMutableConfigValues
    : public DataReductionProxyConfigValues {
 public:
  // Creates a new |DataReductionProxyMutableConfigValues| using |params| as
  // the basis for its initial values.
  static scoped_ptr<DataReductionProxyMutableConfigValues> CreateFromParams(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const DataReductionProxyParams* params);

  ~DataReductionProxyMutableConfigValues() override;

  // Updates |origin_| and  |fallback_origin_| with the provided values.
  // Virtual for testing.
  virtual void UpdateValues(const net::ProxyServer& origin,
                            const net::ProxyServer& fallback_origin);

  // Overrides of |DataReductionProxyConfigValues|
  bool promo_allowed() const override;
  bool holdback() const override;
  bool allowed() const override;
  bool fallback_allowed() const override;
  bool alternative_allowed() const override;
  bool alternative_fallback_allowed() const override;
  bool UsingHTTPTunnel(const net::HostPortPair& proxy_server) const override;
  bool IsDataReductionProxy(
      const net::HostPortPair& host_port_pair,
      DataReductionProxyTypeInfo* proxy_info) const override;
  const net::ProxyServer& origin() const override;
  const net::ProxyServer& fallback_origin() const override;
  const net::ProxyServer& alt_origin() const override;
  const net::ProxyServer& alt_fallback_origin() const override;
  const net::ProxyServer& ssl_origin() const override;
  const GURL& secure_proxy_check_url() const override;

 protected:
  DataReductionProxyMutableConfigValues(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

 private:
  net::ProxyServer empty_origin_;
  bool promo_allowed_;
  bool holdback_;
  bool allowed_;
  bool fallback_allowed_;
  net::ProxyServer origin_;
  net::ProxyServer fallback_origin_;
  GURL secure_proxy_check_url_;

  // |io_task_runner_| should be the task runner for running operations on the
  // IO thread.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMutableConfigValues);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_
