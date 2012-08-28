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
    this.baseURL_ = baseUrl;

    /**
     * Last received XSRF token. Sent as a parameter in every request.
     * @type {string}
     * @private
     */
    this.xsrfToken_ = '';
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
   * @private
   */
  CloudPrintInterface.URL_ENCODED_CONTENT_TYPE_ =
      'application/x-www-form-urlencoded';

  /**
   * Content type header value for a multipart HTTP request.
   * @type {string}
   * @private
   */
  CloudPrintInterface.MULTIPART_CONTENT_TYPE_ =
      'multipart/form-data; boundary=----CloudPrintFormBoundaryjc9wuprokl8i';

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

    /**
     * Sends a Google Cloud Print search API request.
     * @param {boolean} isRecent Whether to search for only recently used
     *     printers.
     */
    search: function(isRecent) {
      var params = {};
      if (isRecent) {
        params['q'] = '^recent';
      }
      params['connection_status'] = 'ALL';
      params['client'] = 'chrome';
      this.sendRequest_('GET', 'search', params, null,
                        this.onSearchDone_.bind(this, isRecent));
    },

    /**
     * Sends a Google Cloud Print submit API request.
     * @param {string} body Body of the HTTP post request to send.
     */
    submit: function(body) {
      this.sendRequest_('POST', 'submit', null, body,
                        this.onSubmitDone_.bind(this));
    },

    /**
     * Sends a Google Cloud Print printer API request.
     * @param {string} printerId ID of the printer to lookup.
     */
    printer: function(printerId) {
      var params = {'printerid': printerId};
      this.sendRequest_('GET', 'printer', params, null,
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
      var params = {
        'printerid': printerId,
        'is_tos_accepted': isAccepted
      };
      this.sendRequest_('POST', 'update', params, null,
                        this.onUpdatePrinterTosAcceptanceDone_.bind(this));
    },

    /**
     * Creates an object that represents a Google Cloud Print print ticket.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to create
     *     the state of the print ticket.
     * @return {!Object} Google Cloud Print print ticket.
     */
    createPrintTicket: function(destination, printTicketStore) {
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

      return {
        'capabilities': ticketItems
      };
    },

    /**
     * Sends a request to the Google Cloud Print API.
     * @param {string} method HTTP method of the request.
     * @param {string} action Google Cloud Print action to perform.
     * @param {Object} params HTTP parameters to include in the request.
     * @param {string} body HTTP multi-part encoded body.
     * @param {function(number, Object)} callback Callback to invoke when
     *     request completes.
     */
    sendRequest_: function(method, action, params, body, callback) {
      if (!this.xsrfToken_) {
        // TODO(rltoscano): Should throw an error if not a read-only action or
        // issue an xsrf token request.
      }
      var url = this.baseURL_ + '/' + action + '?xsrf=' + this.xsrfToken_;

      if (params) {
        for (var paramName in params) {
          url += '&' + paramName + '=' + encodeURIComponent(params[paramName]);
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
      if (status == 200 && result['success']) {
        var printerListJson = result['printers'] || [];
        var printerList = [];
        printerListJson.forEach(function(printerJson) {
          try {
            printerList.push(
                cloudprint.CloudDestinationParser.parse(printerJson));
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
          printer = cloudprint.CloudDestinationParser.parse(printerJson);
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

  // Export
  return {
    CloudPrintInterface: CloudPrintInterface
  };
});
