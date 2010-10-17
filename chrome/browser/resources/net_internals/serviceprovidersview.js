// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays information on Winsock layered service providers and
 * namespace providers.
 *
 * For each layered service provider, shows the name, dll, and type
 * information.  For each namespace provider, shows the name and
 * whether or not it's active.
 *
 * @constructor
 */
function ServiceProvidersView(tabId,
                              mainBoxId,
                              serviceProvidersTbodyId,
                              namespaceProvidersTbodyId) {
  DivView.call(this, mainBoxId);

  var tab = document.getElementById(tabId);
  setNodeDisplay(tab, true);

  this.serviceProvidersTbody_ =
      document.getElementById(serviceProvidersTbodyId);
  this.namespaceProvidersTbody_ =
      document.getElementById(namespaceProvidersTbodyId);

  g_browser.addServiceProvidersObserver(this);
}

inherits(ServiceProvidersView, DivView);

ServiceProvidersView.prototype.onServiceProvidersChanged =
function(serviceProviders) {
  this.updateServiceProviders_(serviceProviders['service_providers']);
  this.updateNamespaceProviders_(serviceProviders['namespace_providers']);
};

/**
 * Returns type of a layered service provider.
 */
ServiceProvidersView.getLayeredServiceProviderType =
function(serviceProvider) {
  if (serviceProvider.chain_length == 0)
    return 'Layer';
  if (serviceProvider.chain_length == 1)
    return 'Base';
  return 'Chain';
};

ServiceProvidersView.namespaceProviderType_ = {
  '12': 'NS_DNS',
  '15': 'NS_NLA',
  '16': 'NS_BTH',
  '32': 'NS_NTDS',
  '37': 'NS_EMAIL',
  '38': 'NS_PNRPNAME',
  '39': 'NS_PNRPCLOUD'
};

/**
 * Returns the type of a namespace provider as a string.
 */
ServiceProvidersView.getNamespaceProviderType = function(namespaceProvider) {
  return tryGetValueWithKey(ServiceProvidersView.namespaceProviderType_,
                            namespaceProvider.type);
};

ServiceProvidersView.socketType_ = {
  '1': 'SOCK_STREAM',
  '2': 'SOCK_DGRAM',
  '3': 'SOCK_RAW',
  '4': 'SOCK_RDM',
  '5': 'SOCK_SEQPACKET'
};

/**
 * Returns socket type of a layered service provider as a string.
 */
ServiceProvidersView.getSocketType = function(layeredServiceProvider) {
  return tryGetValueWithKey(ServiceProvidersView.socketType_,
                            layeredServiceProvider.socket_type);
};

ServiceProvidersView.protocolType_ = {
  '1': 'IPPROTO_ICMP',
  '6': 'IPPROTO_TCP',
  '17': 'IPPROTO_UDP',
  '58': 'IPPROTO_ICMPV6'
};

/**
 * Returns protocol type of a layered service provider as a string.
 */
ServiceProvidersView.getProtocolType = function(layeredServiceProvider) {
  return tryGetValueWithKey(ServiceProvidersView.protocolType_,
                            layeredServiceProvider.socket_protocol);
};

 /**
 * Updates the table of layered service providers.
 */
ServiceProvidersView.prototype.updateServiceProviders_ =
function(serviceProviders) {
  this.serviceProvidersTbody_.innerHTML = '';

  // Add a table row for each service provider.
  for (var i = 0; i < serviceProviders.length; ++i) {
    var tr = addNode(this.serviceProvidersTbody_, 'tr');
    var entry = serviceProviders[i];

    addNodeWithText(tr, 'td', entry.name);
    addNodeWithText(tr, 'td', entry.version);
    addNodeWithText(tr, 'td',
                    ServiceProvidersView.getLayeredServiceProviderType(entry));
    addNodeWithText(tr, 'td', ServiceProvidersView.getSocketType(entry));
    addNodeWithText(tr, 'td', ServiceProvidersView.getProtocolType(entry));
    addNodeWithText(tr, 'td', entry.path);
  }
};

/**
 * Updates the lable of namespace providers.
 */
ServiceProvidersView.prototype.updateNamespaceProviders_ =
function(namespaceProviders) {
  this.namespaceProvidersTbody_.innerHTML = '';

  // Add a table row for each namespace provider.
  for (var i = 0; i < namespaceProviders.length; ++i) {
    var tr = addNode(this.namespaceProvidersTbody_, 'tr');
    var entry = namespaceProviders[i];
    addNodeWithText(tr, 'td', entry.name);
    addNodeWithText(tr, 'td', entry.version);
    addNodeWithText(tr, 'td',
                    ServiceProvidersView.getNamespaceProviderType(entry));
    addNodeWithText(tr, 'td', entry.active);
  }
};

