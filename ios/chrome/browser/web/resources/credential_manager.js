// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript implementation of the credential management API
 * defined at http://w3c.github.io/webappsec-credential-management
 * This is a minimal implementation that sends data to the app side to
 * integrate with the password manager. When loaded, installs the API onto
 * the window.navigator.object.
 */

// TODO(crbug.com/435046) tgarbus: This only contains method signatures.
// Implement the JavaScript API and types according to the spec:
// https://w3c.github.io/webappsec-credential-management


// Namespace for credential management. __gCrWeb must have already
// been defined.
__gCrWeb['credentialManager'] = {
};

/**
 * The Credential interface, for more information see
 * https://w3c.github.io/webappsec-credential-management/#the-credential-interface
 * @constructor
 */
function Credential() {}

/**
 * The CredentialRequestOptions dictionary, for more information see
 * https://w3c.github.io/webappsec-credential-management/#credentialrequestoptions-dictionary
 * @typedef {{mediation: string}}
 */
var CredentialRequestOptions;

/**
 * The CredentialCreationOptions dictionary, for more information see
 * https://w3c.github.io/webappsec-credential-management/#credentialcreationoptions-dictionary
 * @typedef {Object}
 */
var CredentialCreationOptions;

/**
 * Implements the public Credential Management API. For more information, see
 * https://w3c.github.io/webappsec-credential-management/#credentialscontainer
 * @constructor
 */
function CredentialsContainer() {}

/**
 * Performs the Request A Credential action described at
 * https://w3c.github.io/webappsec-credential-management/#abstract-opdef-request-a-credential
 * @param {!CredentialRequestOptions} options An optional dictionary of
 *     parameters for the request.
 * @return {!Promise<?Credential>} A promise for retrieving the result
 *     of the request.
 */
CredentialsContainer.prototype.get = function(options) {
  // TODO(crbug.com/435046) tgarbus: Implement |get| as JS invoking
  // |get| on host.
};

/**
 * Performs the Store A Credential action described at
 * https://w3c.github.io/webappsec-credential-management/#abstract-opdef-store-a-credential
 * @param {!Credential} credential A credential object to store.
 * @return {!Promise<void>} A promise for retrieving the result
 *     of the request.
 */
CredentialsContainer.prototype.store = function(credential) {
  // TODO(crbug.com/435046) tgarbus: Implement |store| as JS invoking
  // |store| on host.
};

/**
 * Performs the Prevent Silent Access action described at
 * https://w3c.github.io/webappsec-credential-management/#abstract-opdef-prevent-silent-access
 * @return {!Promise<void>} A promise for retrieving the result
 *     of the request.
 */
CredentialsContainer.prototype.preventSilentAccess = function() {
  // TODO(crbug.com/435046) tgarbus: Implement |preventSilentAccess| as JS
  // invoking |preventSilentAccess| on host.
};

/**
 * Performs the Create A Credential action described at
 * https://w3c.github.io/webappsec-credential-management/#abstract-opdef-create-a-credential
 * @param {!CredentialCreationOptions} options An optional dictionary of
 *     of params for creating a new Credential object.
 * @return {!Promise<?Credential>} A promise for retrieving the result
 *     of the request.
 */
CredentialsContainer.prototype.create = function(options) {
  // TODO(crbug.com/435046) tgarbus: Implement |create| as JS function
};

// Install the public interface.
window.navigator.credentials = new CredentialsContainer();
