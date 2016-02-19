// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript implementation of the credential management API
 * defined at http://w3c.github.io/webappsec/specs/credentialmanagement.
 * This is a minimal implementation that sends data to the app side to
 * integrate with the password manager. When loaded, installs the API onto
 * the window.navigator object.
 */

// Namespace for all credential management stuff. __gCrWeb must have already
// been defined.
__gCrWeb['credentialManager'] = {
  /**
   * The next ID for forwarding a call from JS to the App and tracking its
   * associated Promise resolvers and rejecters.
   * @private {number}
   */
  nextId_: 0,

  /**
   * Tracks navigator.credentials Promise resolvers.
   * @type {!Object<number, function(?Credential|undefined)>}
   * @private
   */
  resolvers_: {},

  /**
   * Tracks navigator.credentials Promise rejecters.
   * @type {!Object<number, function(?Error)>}
   * @private
   */
  rejecters_: {}
};


/** @enum {string} */
__gCrWeb.credentialManager.CredentialType = {
  CREDENTIAL: 'Credential',
  PASSWORD_CREDENTIAL: 'PasswordCredential',
  FEDERATED_CREDENTIAL: 'FederatedCredential',
  PENDING_CREDENTIAL: 'PendingCredential'
};


/** @typedef {
 *     type: __gCrWeb.credentialManager.CredentialType,
 *     id: string,
 *     name: (string|undefined),
 *     avatarURL: (string|undefined),
 *     federation: (string|undefined)
 * }
 */
__gCrWeb.credentialManager.SerializedCredential;


/**
 * Creates and returns a Promise whose resolver and rejecter functions are
 * stored with the associated |requestId|. They can be accessed by calling
 * |resolve| or |reject| on __gCrWeb['credentialManager'] with that ID.
 * @param {number} requestId An identifier to track the resolver and rejecter
 *     associated with the returned Promise.
 * @return {!Promise<?Credential|undefined>} A new Promise.
 * @private
 */
__gCrWeb['credentialManager'].createPromise_ = function(requestId) {
  return new Promise(function(resolve, reject) {
    __gCrWeb['credentialManager'].resolvers_[requestId] = resolve;
    __gCrWeb['credentialManager'].rejecters_[requestId] = reject;
  });
};


/**
 * Deletes the resolver and rejecter of the Promise associated with |requestId|.
 * @param {number} requestId The identifier of the Promise.
 * @private
 */
__gCrWeb['credentialManager'].removePromise_ = function(requestId) {
  delete __gCrWeb['credentialManager'].rejecters_[requestId];
  delete __gCrWeb['credentialManager'].resolvers_[requestId];
};


/**
 * Parses |credentialData| into a Credential object.
 * @param {!__gCrWeb.credentialManager.SerializedCredential} credentialData A
 *     simple object representation of a Credential like might be obtained from
 *     Credential.prototype.serialize.
 * @return {?Credential} A Credential object, or null if parsing was
 *     unsuccessful.
 * @private
 */
__gCrWeb['credentialManager'].parseCredential_ = function(credentialData) {
  var CredentialType = __gCrWeb.credentialManager.CredentialType;
  switch (credentialData['type']) {
    case CredentialType.CREDENTIAL:
      return Credential.parse(credentialData);
    case CredentialType.PASSWORD_CREDENTIAL:
      return PasswordCredential.parse(credentialData);
    case CredentialType.FEDERATED_CREDENTIAL:
      return FederatedCredential.parse(credentialData);
    case CredentialType.PENDING_CREDENTIAL:
      return PendingCredential.parse(credentialData);
    default:
      return null;
  }
};


/**
 * Resolves the Promise that was created with the given |requestId| with a
 * Credential object parsed from |opt_credentialData| and removes that promise
 * from the global state. Future attempts to resolve or reject the Promise will
 * fail.
 * @param {number} requestId The identifier of the Promise to resolve.
 * @param {Object=} opt_credentialData An object describing a credential. If
 *     provided, this parameter will be parsed into the appropriate Credential
 *     type.
 * @return {boolean} Indicates whether the Promise was successfully resolved.
 */
__gCrWeb['credentialManager'].resolve = function(requestId,
                                                 opt_credentialData) {
  var resolver = __gCrWeb['credentialManager'].resolvers_[requestId];
  if (!resolver) {
    return false;
  }
  if (opt_credentialData) {
    var credential = null;
    try {
      credential =
          __gCrWeb['credentialManager'].parseCredential_(opt_credentialData);
    } catch (e) {
      // Failed to parse |opt_credentialData|. The app side sent bad data.
      return false;
    }
    resolver(credential);
  } else {
    resolver();
  }
  __gCrWeb['credentialManager'].removePromise_(requestId);
  return true;
};


/**
 * Rejects the Promise that was created with the given |requestId| and cleans up
 * that Promise.
 * @param {number} requestId The identifier of the Promise to resolve.
 * @param {string} errorType The type of the Error to pass to the rejecter
 *     function. If not recognized, a standard JavaScript Error will be used.
 * @param {string} message A human-readable description of the error.
 * @return {boolean} Indicates whether the Promise was successfully rejected.
 */
__gCrWeb['credentialManager'].reject = function(requestId, errorType, message) {
  var rejecter = __gCrWeb['credentialManager'].rejecters_[requestId];
  if (!rejecter) {
    return false;
  }
  var error = null;
  if (errorType == 'SecurityError') {
    error = new SecurityError(message);
  } else if (errorType == 'InvalidStateError') {
    error = new InvalidStateError(message);
  } else {
    error = new Error(message);
  }
  rejecter(error);
  __gCrWeb['credentialManager'].removePromise_(requestId);
  return true;
};


/**
 * Sends a command representing |method| of navigator.credentials to the app
 * side with the given |opt_options|.
 * @param {string} command The name of the command being sent.
 * @param {Object=} opt_options A dictionary of additional properties to forward
 *     to the app.
 * @return {!Promise<?Credential|undefined>} A promise for the result.
 * @private
 */
__gCrWeb['credentialManager'].invokeOnHost_ = function(command, opt_options) {
  var requestId = __gCrWeb['credentialManager'].nextId_++;
  var message = {
    'command': command,
    'requestId': requestId
  };
  if (opt_options) {
    Object.keys(opt_options).forEach(function(key) {
      message[key] = opt_options[key];
    });
  }
  __gCrWeb.message.invokeOnHost(message);
  return __gCrWeb['credentialManager'].createPromise_(requestId);
};



/**
 * Creates a new SecurityError to represent failures caused by violating
 * security requirements of the API.
 * @param {string} message A human-readable message describing this error.
 * @extends {Error}
 * @constructor
 */
function SecurityError(message) {
  Error.call(this, message);
  this.name = 'SecurityError';
  this.message = message;
}
SecurityError.prototype = Object.create(Error.prototype);
SecurityError.prototype.constructor = SecurityError;



/**
 * Creates a new InvalidStateError to represent failures caused by inconsistent
 * internal state.
 * @param {string} message A human-readable message describing this error.
 * @extends {Error}
 * @constructor
 */
function InvalidStateError(message) {
  Error.call(this, message);
  this.name = 'InvalidStateError';
  this.message = message;
}
InvalidStateError.prototype = Object.create(Error.prototype);
InvalidStateError.prototype.constructor = InvalidStateError;



/**
 * Creates a new Credential object. For more information, see
 * https://w3c.github.io/webappsec/specs/credentialmanagement/#credential
 * @param {string} id The credential’s identifier. This might be a username or
 *     email address, for instance.
 * @param {string=} opt_name A name associated with the credential, intended as
 *     a human-understandable public name.
 * @param {string=} opt_avatarUrl A URL pointing to an avatar image for the
 *     user. This URL MUST NOT be an a priori insecure URL.
 * @constructor
 */
function Credential(id, opt_name, opt_avatarUrl) {
  if (id === null || id === undefined)
    throw new TypeError('id must be provided');
  /**
   * The credential's identifier. Read-only.
   * @type {string}
   */
  this.id;
  Object.defineProperty(this, 'id', {
    configurable: false,
    enumerable: true,
    value: id,
    writable: false
  });
  /**
   * A human-understandable public name associated with the credential.
   * Read-only.
   * @type {string}
   */
  this.name;
  Object.defineProperty(this, 'name', {
    configurable: false,
    enumerable: true,
    value: opt_name || '',
    writable: false
  });
  /**
   * A URL pointing to an avatar image for the user. Read-only.
   * NOTE: This property name deviates from the Google JavaScript style guide
   * in order to meet the public API specification.
   * @type {string}
   */
  this.avatarURL;
  Object.defineProperty(this, 'avatarURL', {
    configurable: false,
    enumerable: true,
    value: opt_avatarUrl || '',
    writable: false
  });
}


/**
 * Returns a simple object representation of this credential suitable for
 * sending to the app side.
 * @return {__gCrWeb.credentialManager.SerializedCredential} An object
 *     representing this credential.
 */
Credential.prototype.serialize = function() {
  var serialized = {
    'type': this.constructor.name,
    'id': this.id,
    'name': this.name
  };
  if (this.avatarURL && this.avatarURL.length > 0)
    serialized['avatarURL'] = this.avatarURL;
  return serialized;
};


/**
 * Parses |credentialData| into a Credential object.
 * @param {!__gCrWeb.credentialManager.SerializedCredential} credentialData A
 *     simple object representation of a Credential like might be obtained from
 *     Credential.prototype.serialize.
 * @return {Credential} A Credential object.
 */
Credential.parse = function(credentialData) {
  return new Credential(credentialData['id'],
                        credentialData['name'],
                        credentialData['avatarURL']);
};



/**
 * Creates a new PasswordCredential object. For more information, see
 * https://w3c.github.io/webappsec/specs/credentialmanagement/#localcredential
 * @param {string} id The credential’s identifier. This might be a username or
 *     email address, for instance.
 * @param {string} password The credential's password.
 * @param {string=} opt_name A name associated with the credential, intended as
 *     a human-understandable public name.
 * @param {string=} opt_avatarUrl A URL pointing to an avatar image for the
 *     user. This URL MUST NOT be an a priori insecure URL.
 * @constructor
 * @extends {Credential}
 */
function PasswordCredential(id, password, opt_name, opt_avatarUrl) {
  if (password === null || password === undefined)
    throw new TypeError('password must be provided');
  Credential.call(this, id, opt_name, opt_avatarUrl);
  var formData = new FormData();
  formData.append('username', id);
  formData.append('password', password);
  /**
   * A FormData object, containing two entries: one named username, the other
   * named password. Read-only.
   * @type {FormData}
   */
  this.formData;
  Object.defineProperty(this, 'formData', {
    configurable: false,
    enumerable: true,
    value: formData,
    writable: false
  });
  /**
   * The credential's password.
   * @type {string}
   * @private
   */
  this.password_;
  Object.defineProperty(this, 'password_', {
    configurable: false,
    enumerable: false,
    value: password,
    writable: false
  });
}
PasswordCredential.prototype = Object.create(Credential.prototype);
PasswordCredential.prototype.constructor = PasswordCredential;


/**
 * Returns a simple object representation of this credential suitable for
 * sending to the app side.
 * @return {__gCrWeb.credentialManager.SerializedCredential} An object
 *     representing this credential.
 */
PasswordCredential.prototype.serialize = function() {
  var obj = Credential.prototype.serialize.call(this);
  obj.password = this.password_;
  return obj;
};


/**
 * Parses |credentialData| into a PasswordCredential object.
 * @param {!__gCrWeb.credentialManager.SerializedCredential} credentialData A
 *     simple object representation of a PasswordCredential like might be
 *     obtained from PasswordCredential.prototype.serialize.
 * @return {PasswordCredential} A PasswordCredential object.
 */
PasswordCredential.parse = function(credentialData) {
  return new PasswordCredential(credentialData['id'],
                                credentialData['password'],
                                credentialData['name'],
                                credentialData['avatarURL']);
};



/**
 * Creates a new FederatedCredential object. For more information, see
 * https://w3c.github.io/webappsec/specs/credentialmanagement/#federatedcredential
 * @param {string} id The credential’s identifier. This might be a username or
 *     email address, for instance.
 * @param {string} federation The credential’s federation. For details regarding
 *     valid formats, see https://w3c.github.io/webappsec/specs/credentialmanagement/#identifying-federations
 * @param {string=} opt_name A name associated with the credential, intended as
 *     a human-understandable public name.
 * @param {string=} opt_avatarUrl A URL pointing to an avatar image for the
 *     user. This URL MUST NOT be an a priori insecure URL.
 * @constructor
 * @extends {Credential}
 */
function FederatedCredential(id, federation, opt_name, opt_avatarUrl) {
  if (federation === null || federation === undefined)
    throw new TypeError('federation must be provided');
  Credential.call(this, id, opt_name, opt_avatarUrl);
  /**
   * The credential’s federation. Read-only.
   * @type {string}
   */
  this.federation;
  Object.defineProperty(this, 'federation', {
    configurable: false,
    enumerable: true,
    value: federation,
    writable: false
  });
}
FederatedCredential.prototype = Object.create(Credential.prototype);
FederatedCredential.prototype.constructor = FederatedCredential;


/**
 * Returns a simple object representation of this credential suitable for
 * sending to the app side.
 * @return {__gCrWeb.credentialManager.SerializedCredential} An object
 *     representing this credential.
 */
FederatedCredential.prototype.serialize = function() {
  var obj = Credential.prototype.serialize.call(this);
  obj.federation = this.federation;
  return obj;
};


/**
 * Parses |credentialData| into a FederatedCredential object.
 * @param {!__gCrWeb.credentialManager.SerializedCredential} credentialData A
 *     simple object representation of a FederatedCredential like might be
 *     obtained from FederatedCredential.prototype.serialize.
 * @return {!FederatedCredential} A FederatedCredential object.
 */
FederatedCredential.parse = function(credentialData) {
  return new FederatedCredential(credentialData['id'],
                                 credentialData['federation'],
                                 credentialData['name'],
                                 credentialData['avatarURL']);
};



/**
 * Creates a new PendingCredential object. For more information, see
 * https://w3c.github.io/webappsec/specs/credentialmanagement/#pendingcredential
 * @param {string} id The credential’s identifier. This might be a username or
 *     email address, for instance.
 * @param {string=} opt_name A name associated with the credential, intended as
 *     a human-understandable public name.
 * @param {string=} opt_avatarUrl A URL pointing to an avatar image for the
 *     user. This URL MUST NOT be an a priori insecure URL.
 * @constructor
 * @extends {Credential}
 */
function PendingCredential(id, opt_name, opt_avatarUrl) {
  Credential.call(this, id, opt_name, opt_avatarUrl);
}
PendingCredential.prototype = Object.create(Credential.prototype);
PendingCredential.prototype.constructor = PendingCredential;


/**
 * Parses |credentialData| into a PendingCredential object.
 * @param {!__gCrWeb.credentialManager.SerializedCredential} credentialData A
 *     simple object representation of a PendingCredential like might be
 *     obtained from PendingCredential.prototype.serialize.
 * @return {!Credential} A PendingCredential object.
 */
PendingCredential.parse = function(credentialData) {
  return new PendingCredential(credentialData['id'],
                               credentialData['name'],
                               credentialData['avatarURL']);
};



/**
 * Implements the public Credential Management API. For more information, see
 * http://w3c.github.io/webappsec/specs/credentialmanagement/#interfaces-credential-manager
 * @constructor
 */
function CredentialsContainer() {
}


/**
 * Requests a credential from the credential manager.
 * @param {{unmediated: boolean, federations: Array<string>}=} opt_options An
 *     optional dictionary of parameters for the request. If |unmediated| is
 *     true, the returned promise will only be resolved with a credential if
 *     this is possible without user interaction; otherwise, the returned
 *     promise will be resolved with |undefined|. |federations| specifies a
 *     list of acceptable federation providers. For more information, see
 *     https://w3c.github.io/webappsec/specs/credentialmanagement/#interfaces-request-options
 * @return {!Promise<?Credential|undefined>} A promise for retrieving the result
 *     of the request.
 */
CredentialsContainer.prototype.request = function(opt_options) {
  var options = {
    'unmediated': !!opt_options && !!opt_options['unmediated'],
    'federations': (!!opt_options && opt_options['federations']) || []
  };
  return __gCrWeb['credentialManager'].invokeOnHost_(
      'navigator.credentials.request', options);
};


/**
 * Notifies the browser that the user has successfully signed in.
 * @param {Credential=} opt_successfulCredential The credential that was used
 *     to sign in.
 * @return {!Promise<?Credential|undefined>} A promise to wait for
 *     acknowledgement from the browser.
 */
CredentialsContainer.prototype.notifySignedIn = function(
    opt_successfulCredential) {
  var options = opt_successfulCredential && {
    'credential': opt_successfulCredential.serialize()
  };
  return __gCrWeb['credentialManager'].invokeOnHost_(
      'navigator.credentials.notifySignedIn', options);
};


/**
 * Notifies the browser that the user failed to sign in.
 * @param {Credential=} opt_failedCredential The credential that failed to
 *     sign in.
 * @return {!Promise<?Credential|undefined>} A promise to wait for
 *     acknowledgement from the browser.
 */
CredentialsContainer.prototype.notifyFailedSignIn = function(
    opt_failedCredential) {
  var options = opt_failedCredential && {
    'credential': opt_failedCredential.serialize()
  };
  return __gCrWeb['credentialManager'].invokeOnHost_(
      'navigator.credentials.notifyFailedSignIn', options);
};


/**
 * Notifies the browser that the user signed out.
 * @return {!Promise<?Credential|undefined>} A promise to wait for
 *     acknowledgement from the browser.
 */
CredentialsContainer.prototype.notifySignedOut = function() {
  return __gCrWeb['credentialManager'].invokeOnHost_(
      'navigator.credentials.notifySignedOut');
};


// Install the public interface.
window.navigator.credentials = new CredentialsContainer();
