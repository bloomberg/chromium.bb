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
  this.updateServiceProviders_(serviceProviders["service_providers"]);
  this.updateNamespaceProviders_(serviceProviders["namespace_providers"]);
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

    if (entry.chain_length == 0)
      addNodeWithText(tr, 'td', 'Layer');
    else if (entry.chain_length == 1)
      addNodeWithText(tr, 'td', 'Base');
    else
      addNodeWithText(tr, 'td', 'Chain');

    addNodeWithText(tr, 'td', entry.socket_type);
    addNodeWithText(tr, 'td', entry.socket_protocol);
    addNodeWithText(tr, 'td', entry.path);
  }
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

    var typeString = ServiceProvidersView.namespaceProviderType_[entry.type];
    if (typeString)
      addNodeWithText(tr, 'td', typeString);
    else
      addNodeWithText(tr, 'td', entry.type);

    addNodeWithText(tr, 'td', entry.active);
  }
};

