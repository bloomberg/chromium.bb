// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on Winsock layered service providers and
 * namespace providers.
 *
 * For each layered service provider, shows the name, dll, and type
 * information.  For each namespace provider, shows the name and
 * whether or not it's active.
 */
var ServiceProvidersView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function ServiceProvidersView() {
    assertFirstConstructorCall(ServiceProvidersView);

    // Call superclass's constructor.
    superClass.call(this, ServiceProvidersView.MAIN_BOX_ID);

    this.serviceProvidersTbody_ =
        $(ServiceProvidersView.SERVICE_PROVIDERS_TBODY_ID);
    this.namespaceProvidersTbody_ =
        $(ServiceProvidersView.NAMESPACE_PROVIDERS_TBODY_ID);

    g_browser.addServiceProvidersObserver(this);
  }

  // ID for special HTML element in category_tabs.html
  ServiceProvidersView.TAB_HANDLE_ID = 'tab-handle-service-providers';

  // IDs for special HTML elements in service_providers_view.html
  ServiceProvidersView.MAIN_BOX_ID = 'service-providers-view-tab-content';
  ServiceProvidersView.SERVICE_PROVIDERS_TBODY_ID =
      'service-providers-view-tbody';
  ServiceProvidersView.NAMESPACE_PROVIDERS_TBODY_ID =
      'service-providers-view-namespace-providers-tbody';

  cr.addSingletonGetter(ServiceProvidersView);

  ServiceProvidersView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onServiceProvidersChanged(data.serviceProviders);
    },

    onServiceProvidersChanged: function(serviceProviders) {
      return serviceProviders &&
          this.updateServiceProviders_(serviceProviders['service_providers']) &&
          this.updateNamespaceProviders_(
              serviceProviders['namespace_providers']);
    },

     /**
     * Updates the table of layered service providers.
     */
    updateServiceProviders_: function(serviceProviders) {
      this.serviceProvidersTbody_.innerHTML = '';

      if (!serviceProviders)
        return false;

      // Add a table row for each service provider.
      for (var i = 0; i < serviceProviders.length; ++i) {
        var tr = addNode(this.serviceProvidersTbody_, 'tr');
        var entry = serviceProviders[i];

        addNodeWithText(tr, 'td', entry.name);
        addNodeWithText(tr, 'td', entry.version);
        addNodeWithText(tr, 'td', getLayeredServiceProviderType(entry));
        addNodeWithText(tr, 'td', getSocketType(entry));
        addNodeWithText(tr, 'td', getProtocolType(entry));
        addNodeWithText(tr, 'td', entry.path);
      }
      return true;
    },

    /**
     * Updates the lable of namespace providers.
     */
    updateNamespaceProviders_: function(namespaceProviders) {
      this.namespaceProvidersTbody_.innerHTML = '';

      if (!namespaceProviders)
        return false;

      // Add a table row for each namespace provider.
      for (var i = 0; i < namespaceProviders.length; ++i) {
        var tr = addNode(this.namespaceProvidersTbody_, 'tr');
        var entry = namespaceProviders[i];
        addNodeWithText(tr, 'td', entry.name);
        addNodeWithText(tr, 'td', entry.version);
        addNodeWithText(tr, 'td', getNamespaceProviderType(entry));
        addNodeWithText(tr, 'td', entry.active);
      }
      return true;
    }
  };

  /**
   * Returns type of a layered service provider.
   */
  function getLayeredServiceProviderType(serviceProvider) {
    if (serviceProvider.chain_length == 0)
      return 'Layer';
    if (serviceProvider.chain_length == 1)
      return 'Base';
    return 'Chain';
  }

  var NAMESPACE_PROVIDER_PTYPE = {
    '12': 'NS_DNS',
    '15': 'NS_NLA',
    '16': 'NS_BTH',
    '32': 'NS_NTDS',
    '37': 'NS_EMAIL',
    '38': 'NS_PNRPNAME',
    '39': 'NS_PNRPCLOUD'
  }

  /**
   * Returns the type of a namespace provider as a string.
   */
  function getNamespaceProviderType(namespaceProvider) {
    return tryGetValueWithKey(NAMESPACE_PROVIDER_PTYPE,
                              namespaceProvider.type);
  };

  var SOCKET_TYPE = {
    '1': 'SOCK_STREAM',
    '2': 'SOCK_DGRAM',
    '3': 'SOCK_RAW',
    '4': 'SOCK_RDM',
    '5': 'SOCK_SEQPACKET'
  };

  /**
   * Returns socket type of a layered service provider as a string.
   */
  function getSocketType(layeredServiceProvider) {
    return tryGetValueWithKey(SOCKET_TYPE,
                              layeredServiceProvider.socket_type);
  }

  var PROTOCOL_TYPE = {
    '1': 'IPPROTO_ICMP',
    '6': 'IPPROTO_TCP',
    '17': 'IPPROTO_UDP',
    '58': 'IPPROTO_ICMPV6'
  };

  /**
   * Returns protocol type of a layered service provider as a string.
   */
  function getProtocolType(layeredServiceProvider) {
    return tryGetValueWithKey(PROTOCOL_TYPE,
                              layeredServiceProvider.socket_protocol);
  }

  return ServiceProvidersView;
})();
