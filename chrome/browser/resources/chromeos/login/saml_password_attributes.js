// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A utility for extracting password information from SAML
 * authorization response. This requires that the SAML IDP administrator
 * has correctly configured their domain to issue these attributes.
 */

cr.define('samlPasswordAttributes', function() {
  'use strict';

  /** @const @private {number} The shortest XML string that could be useful. */
  const MIN_SANE_XML_LENGTH = 100;

  /** @const @private {number} The max length that we are willing to parse. */
  const MAX_SANE_XML_LENGTH = 50 * 1024;  // 50 KB

  /** @const @private {string} Schema name for password modified timestamp. */
  const PASSWORD_MODIFIED_TIMESTAMP_NAME =
      'http://schemas.google.com/saml/2019/passwordmodifiedtimestamp';

  /** @const @private {string} Schema name for password expiration timestamp. */
  const PASSWORD_EXPIRATION_TIMESTAMP_NAME =
      'http://schemas.google.com/saml/2019/passwordexpirationtimestamp';

  /**
   * Query selector for finding an element with tag AttributeValue that is a
   * child of an element of tag Attribute with a certain Name attribute.
   * @const @private {string}
   */
  const QUERY_SELECTOR_FORMAT = 'Attribute[Name="{0}"] > AttributeValue';

  /** @const @private {string} Query selector for password modified time. */
  const PASSWORD_MODIFIED_TIMESTAMP_SELECTOR =
      QUERY_SELECTOR_FORMAT.replace('{0}', PASSWORD_MODIFIED_TIMESTAMP_NAME);

  /** @const @private {string} Query selector for password expiration time. */
  const PASSWORD_EXPIRATION_TIMESTAMP_SELECTOR =
      QUERY_SELECTOR_FORMAT.replace('{0}', PASSWORD_EXPIRATION_TIMESTAMP_NAME);

  /**
   * Extract password information from the Attribute elements in the given SAML
   * authorization response.
   * @param {string} xmlStr The SAML response XML, as a string.
   * @return {!PasswordAttributes} The password information extracted.
   */
  function readPasswordAttributes(xmlStr) {
    if (xmlStr.length < MIN_SANE_XML_LENGTH ||
        xmlStr.length > MAX_SANE_XML_LENGTH) {
      return new PasswordAttributes(null, null);
    }
    const xmlDom = new DOMParser().parseFromString(xmlStr, 'text/xml');
    if (!xmlDom) {
      return new PasswordAttributes(null, null);
    }

    return new PasswordAttributes(
        extractTimestampFromXml(xmlDom, PASSWORD_MODIFIED_TIMESTAMP_SELECTOR),
        extractTimestampFromXml(xmlDom, PASSWORD_EXPIRATION_TIMESTAMP_SELECTOR)
    );
  }

  /**
   * Extracts a timestamp from the given XML DOM, using the given query selector
   * to find it and using {@code samlTimestamps.decodeTimestamp} to decode it.
   * @param {!XMLDocument} xmlDom The XML DOM.
   * @param {string} querySelectorStr The query selector to find the timestamp.
   * @return {?Date} The decoded timestamp (null if failed to extract).
   */
  function extractTimestampFromXml(xmlDom, querySelectorStr) {
    const element = xmlDom.querySelector(querySelectorStr);
    const valueText = element && element.textContent;
    return valueText ? samlTimestamps.decodeTimestamp(valueText) : null;
  }

  /**
   * Struct to hold password attributes.
   * @export @final
   */
  class PasswordAttributes {
    constructor(modifiedTimestamp, expirationTimestamp) {
      /** @type {?Date} Password last-modified timestamp. */
      this.modifiedTimestamp = modifiedTimestamp;

      /** @type {?Date} Password expiration timestamp. */
      this.expirationTimestamp = expirationTimestamp;
    }
  }

  // Public functions:
  return {
    readPasswordAttributes: readPasswordAttributes,
    PasswordAttributes: PasswordAttributes,
  };
});
