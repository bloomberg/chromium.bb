// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /**
   * API to the Google Cloud Print service.
   * @param {string} baseUrl Base part of the Google Cloud Print service URL
   *     with no trailing slash. For example,
   *     'https://www.google.com/cloudprint'.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function CloudPrintInterface(baseUrl) {
    /**
     * The base URL of the Google Cloud Print API.
     * @type {string}
     * @private
     */
    this.baseUrl_ = baseUrl;

    /**
     * Last received XSRF token. Sent as a parameter in every request.
     * @type {string}
     * @private
     */
    this.xsrfToken_ = '';

    /**
     * Number of outstanding cloud destination search requests.
     * @type {number}
     * @private
     */
    this.outstandingCloudSearchRequestCount_ = 0;
  };

  /**
   * Event types dispatched by the interface.
   * @enum {string}
   */
  CloudPrintInterface.EventType = {
    PRINTER_DONE: 'cloudprint.CloudPrintInterface.PRINTER_DONE',
    PRINTER_FAILED: 'cloudprint.CloudPrintInterface.PRINTER_FAILED',
    SEARCH_DONE: 'cloudprint.CloudPrintInterface.SEARCH_DONE',
    SEARCH_FAILED: 'cloudprint.CloudPrintInterface.SEARCH_FAILED',
    SUBMIT_DONE: 'cloudprint.CloudPrintInterface.SUBMIT_DONE',
    SUBMIT_FAILED: 'cloudprint.CloudPrintInterface.SUBMIT_FAILED',
    UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED:
        'cloudprint.CloudPrintInterface.UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED'
  };

  /**
   * Content type header value for a URL encoded HTTP request.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.URL_ENCODED_CONTENT_TYPE_ =
      'application/x-www-form-urlencoded';

  /**
   * Multi-part POST request boundary used in communication with Google
   * Cloud Print.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.MULTIPART_BOUNDARY_ =
      '----CloudPrintFormBoundaryjc9wuprokl8i';

  /**
   * Content type header value for a multipart HTTP request.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.MULTIPART_CONTENT_TYPE_ =
      'multipart/form-data; boundary=' +
      CloudPrintInterface.MULTIPART_BOUNDARY_;

  /**
   * Regex that extracts Chrome's version from the user-agent string.
   * @type {!RegExp}
   * @const
   * @private
   */
  CloudPrintInterface.VERSION_REGEXP_ = /.*Chrome\/([\d\.]+)/i;

  /**
   * Enumeration of JSON response fields from Google Cloud Print API.
   * @enum {string}
   * @private
   */
  CloudPrintInterface.JsonFields_ = {
    PRINTER: 'printer'
  };

  CloudPrintInterface.prototype = {
    __proto__: cr.EventTarget.prototype,

    /** @return {string} Base URL of the Google Cloud Print service. */
    get baseUrl() {
      return this.baseUrl_;
    },

    /**
     * @return {boolean} Whether a search for cloud destinations is in progress.
     */
    get isCloudDestinationSearchInProgress() {
      return this.outstandingCloudSearchRequestCount_ > 0;
    },

    /**
     * Sends a Google Cloud Print search API request.
     * @param {boolean} isRecent Whether to search for only recently used
     *     printers.
     */
    search: function(isRecent) {
      var params = [
        new HttpParam('connection_status', 'ALL'),
        new HttpParam('client', 'chrome')
      ];
      if (isRecent) {
        params.push(new HttpParam('q', '^recent'));
      }
      ++this.outstandingCloudSearchRequestCount_;
      this.sendRequest_('GET', 'search', params,
                        this.onSearchDone_.bind(this, isRecent));
    },

    /**
     * Sends a Google Cloud Print submit API request.
     * @param {!print_preview.Destination} destination Cloud destination to
     *     print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Contains the
     *     print ticket to print.
     * @param {string} data Base64 encoded data of the document.
     */
    submit: function(destination, printTicketStore, data) {
      var result =
          CloudPrintInterface.VERSION_REGEXP_.exec(navigator.userAgent);
      var chromeVersion = 'unknown';
      if (result && result.length == 2) {
        chromeVersion = result[1];
      }
      var params = [
        new HttpParam('printerid', destination.id),
        new HttpParam('contentType', 'dataUrl'),
        new HttpParam('title', printTicketStore.getDocumentTitle()),
        new HttpParam('capabilities',
                      this.createPrintTicket_(destination, printTicketStore)),
        new HttpParam('content', 'data:application/pdf;base64,' + data),
        new HttpParam('tag',
                      '__google__chrome_version=' + chromeVersion),
        new HttpParam('tag', '__google__os=' + navigator.platform)
      ];
      this.sendRequest_('POST', 'submit', params,
                        this.onSubmitDone_.bind(this));
    },

    /**
     * Sends a Google Cloud Print printer API request.
     * @param {string} printerId ID of the printer to lookup.
     */
    printer: function(printerId) {
      var params = [new HttpParam('printerid', printerId)];
      this.sendRequest_('GET', 'printer', params,
                        this.onPrinterDone_.bind(this, printerId));
    },

    /**
     * Sends a Google Cloud Print update API request to accept (or reject) the
     * terms-of-service of the given printer.
     * @param {string} printerId ID of the printer to accept the
     *     terms-of-service for.
     * @param {boolean} isAccepted Whether the user accepted the
     *     terms-of-service.
     */
    updatePrinterTosAcceptance: function(printerId, isAccepted) {
      var params = [
        new HttpParam('printerid', printerId),
        new HttpParam('is_tos_accepted', isAccepted)
      ];
      this.sendRequest_('POST', 'update', params,
                        this.onUpdatePrinterTosAcceptanceDone_.bind(this));
    },

    /**
     * Creates an object that represents a Google Cloud Print print ticket.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to create
     *     the state of the print ticket.
     * @return {!Object} Google Cloud Print print ticket.
     * @private
     */
    createPrintTicket_: function(destination, printTicketStore) {
      assert(!destination.isLocal,
             'Trying to create a Google Cloud Print print ticket for a local ' +
                 'destination');
      assert(destination.capabilities,
             'Trying to create a Google Cloud Print print ticket for a ' +
                 'destination with no print capabilities');

      var ticketItems = [];

      if (destination.capabilities.collateCapability) {
        var collateCap = destination.capabilities.collateCapability;
        var ticketItem = {
          'name': collateCap.id,
          'type': collateCap.type,
          'options': [{'name': printTicketStore.isCollateEnabled() ?
              collateCap.collateOption : collateCap.noCollateOption}]
        };
        ticketItems.push(ticketItem);
      }

      if (destination.capabilities.colorCapability) {
        var colorCap = destination.capabilities.colorCapability;
        var ticketItem = {
          'name': colorCap.id,
          'type': colorCap.type,
          'options': [{'name': printTicketStore.isColorEnabled() ?
              colorCap.colorOption : colorCap.bwOption}]
        };
        ticketItems.push(ticketItem);
      }

      if (destination.capabilities.copiesCapability) {
        var copiesCap = destination.capabilities.copiesCapability;
        var ticketItem = {
          'name': copiesCap.id,
          'type': copiesCap.type,
          'value': printTicketStore.getCopies()
        };
        ticketItems.push(ticketItem);
      }

      if (destination.capabilities.duplexCapability) {
        var duplexCap = destination.capabilities.duplexCapability;
        var ticketItem = {
          'name': duplexCap.id,
          'type': duplexCap.type,
          'options': [{'name': printTicketStore.isDuplexEnabled() ?
              duplexCap.longEdgeOption : duplexCap.simplexOption}]
        };
        ticketItems.push(ticketItem);
      }

      return JSON.stringify({'capabilities': ticketItems});
    },

    /**
     * Sends a request to the Google Cloud Print API.
     * @param {string} method HTTP method of the request.
     * @param {string} action Google Cloud Print action to perform.
     * @param {Array.<!HttpParam>} params HTTP parameters to include in the
     *     request.
     * @param {function(number, Object)} callback Callback to invoke when
     *     request completes.
     */
    sendRequest_: function(method, action, params, callback) {
      if (!this.xsrfToken_) {
        // TODO(rltoscano): Should throw an error if not a read-only action or
        // issue an xsrf token request.
      }
      var url = this.baseUrl_ + '/' + action + '?xsrf=' + this.xsrfToken_;
      var body = null;

      if (params) {
        if (method == 'GET') {
          url = params.reduce(function(partialUrl, param) {
            return partialUrl + '&' + param.name + '=' +
                encodeURIComponent(param.value);
          }, url);
        } else if (method == 'POST') {
          body = params.reduce(function(partialBody, param) {
            return partialBody + 'Content-Disposition: form-data; name=\"' +
                param.name + '\"\r\n\r\n' + param.value + '\r\n--' +
                CloudPrintInterface.MULTIPART_BOUNDARY_ + '\r\n';
          }, '--' + CloudPrintInterface.MULTIPART_BOUNDARY_ + '\r\n');
        }
      }

      var headers = {};
      headers['X-CloudPrint-Proxy'] = 'ChromePrintPreview';
      if (method == 'GET') {
        headers['Content-Type'] = CloudPrintInterface.URL_ENCODED_CONTENT_TYPE_;
      } else if (method == 'POST') {
        headers['Content-Type'] = CloudPrintInterface.MULTIPART_CONTENT_TYPE_;
      }

      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange =
          this.onReadyStateChange_.bind(this, xhr, callback);
      xhr.open(method, url, true);
      xhr.withCredentials = true;
      for (var header in headers) {
        xhr.setRequestHeader(header, headers[header]);
      }
      xhr.send(body);
    },

    /**
     * Creates a Google Cloud Print interface error that is ready to dispatch.
     * @param {!CloudPrintInterface.EventType} type Type of the error.
     * @param {number} status HTTP status code of the failed request.
     * @param {Object} result JSON response of the request. {@code null} if
     *     status was not 200.
     * @return {!cr.Event} Google Cloud Print interface error event.
     * @private
     */
    createErrorEvent_: function(type, status, result) {
      var errorEvent = new cr.Event(type);
      errorEvent.status = status;
      errorEvent.errorCode = status == 200 ? result['errorCode'] : 0;
      errorEvent.message = status == 200 ? result['message'] : '';
      return errorEvent;
    },

    /**
     * Called when the ready-state of a XML http request changes.
     * Calls the successCallback with the result or dispatches an ERROR event.
     * @param {XMLHttpRequest} xhr XML http request that changed.
     * @param {function(number, Object)} callback Callback to invoke when
     *     request completes.
     * @private
     */
    onReadyStateChange_: function(xhr, callback) {
      if (xhr.readyState == 4) {
        if (xhr.status == 200) {
          var result = JSON.parse(xhr.responseText);
          if (result['success']) {
            this.xsrfToken_ = result['xsrf_token'];
          }
        }
        callback(xhr.status, result);
      }
    },

    /**
     * Called when the search request completes.
     * @param {boolean} isRecent Whether the search request was for recent
     *     destinations.
     * @param {number} status Status of the HTTP request.
     * @param {Object} result JSON response.
     * @private
     */
    onSearchDone_: function(isRecent, status, result) {
      --this.outstandingCloudSearchRequestCount_;
      if (status == 200 && result['success']) {
        var printerListJson = result['printers'] || [];
        var printerList = [];
        printerListJson.forEach(function(printerJson) {
          try {
            printerList.push(
                cloudprint.CloudDestinationParser.parse(
                    printerJson, print_preview.Destination.AuthType.COOKIES));
          } catch (err) {
            console.error('Unable to parse cloud print destination: ' + err);
          }
        });
        var searchDoneEvent =
            new cr.Event(CloudPrintInterface.EventType.SEARCH_DONE);
        searchDoneEvent.printers = printerList;
        searchDoneEvent.isRecent = isRecent;
        searchDoneEvent.email = result['request']['user'];
        this.dispatchEvent(searchDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SEARCH_FAILED, status, result);
        this.dispatchEvent(errorEvent);
      }
    },

    /**
     * Called when the submit request completes.
     * @param {number} status Status of the HTTP request.
     * @param {Object} result JSON response.
     * @private
     */
    onSubmitDone_: function(status, result) {
      if (status == 200 && result['success']) {
        var submitDoneEvent = new cr.Event(
            CloudPrintInterface.EventType.SUBMIT_DONE);
        submitDoneEvent.jobId = result['job']['id'];
        this.dispatchEvent(submitDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SUBMIT_FAILED, status, result);
        this.dispatchEvent(errorEvent);
      }
    },

    /**
     * Called when the printer request completes.
     * @param {string} destinationId ID of the destination that was looked up.
     * @param {number} status Status of the HTTP request.
     * @param {Object} result JSON response.
     * @private
     */
    onPrinterDone_: function(destinationId, status, result) {
      if (status == 200 && result['success']) {
        var printerJson = result['printers'][0];
        var printer;
        try {
          printer = cloudprint.CloudDestinationParser.parse(
              printerJson, print_preview.Destination.AuthType.COOKIES);
        } catch (err) {
          console.error('Failed to parse cloud print destination: ' +
              JSON.stringify(printerJson));
          return;
        }
        var printerDoneEvent =
            new cr.Event(CloudPrintInterface.EventType.PRINTER_DONE);
        printerDoneEvent.printer = printer;
        this.dispatchEvent(printerDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.PRINTER_FAILED, status, result);
        errorEvent.destinationId = destinationId;
        this.dispatchEvent(errorEvent);
      }
    },

    /**
     * Called when the update printer TOS acceptance request completes.
     * @param {number} status Status of the HTTP request.
     * @param {Object} result JSON response.
     * @private
     */
    onUpdatePrinterTosAcceptanceDone_: function(status, result) {
      if (status == 200 && result['success']) {
        // Do nothing.
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SUBMIT_FAILED, status, result);
        this.dispatchEvent(errorEvent);
      }
    }
  };

  /**
   * Data structure that represents an HTTP parameter.
   * @param {string} name Name of the parameter.
   * @param {string} value Value of the parameter.
   * @constructor
   */
  function HttpParam(name, value) {
    /**
     * Name of the parameter.
     * @type {string}
     */
    this.name = name;

    /**
     * Name of the value.
     * @type {string}
     */
    this.value = value;
  };

  // Export
  return {
    CloudPrintInterface: CloudPrintInterface
  };
});
