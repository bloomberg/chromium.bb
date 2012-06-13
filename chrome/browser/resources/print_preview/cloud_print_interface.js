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
    ERROR: 'cloudprint.CloudPrintInterface.ERROR',
    PRINTER_DONE: 'cloudprint.CloudPrintInterface.PRINTER_DONE',
    SEARCH_DONE: 'cloudprint.CloudPrintInterface.SEARCH_DONE',
    SUBMIT_DONE: 'cloudprint.CloudPrintInterface.SUBMIT_DONE'
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
      this.sendRequest_('GET', 'search', params, null, this.onSearchDone_);
    },

    /**
     * Sends a Google Cloud Print submit API request.
     * @param {string} body Body of the HTTP post request to send.
     */
    submit: function(body) {
      this.sendRequest_('POST', 'submit', null, body, this.onSubmitDone_);
    },

    /**
     * Sends a Google Cloud Print printer API request.
     * @param {string} printerId ID of the printer to lookup.
     */
    printer: function(printerId) {
      var params = {'printerid': printerId};
      this.sendRequest_('GET', 'printer', params, null, this.onPrinterDone_);
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
                        this.onUpdatePrinterTosAcceptanceDone_);
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
     * @param {function(Object)} successCallback Callback to invoke when request
     *     completes successfully.
     */
    sendRequest_: function(method, action, params, body, successCallback) {
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
      xhr.onreadystatechange = this.onReadyStateChange_.bind(
          this, xhr, successCallback.bind(this));
      xhr.open(method, url, true);
      xhr.withCredentials = true;
      for (var header in headers) {
        xhr.setRequestHeader(header, headers[header]);
      }
      xhr.send(body);
    },

    /**
     * Dispatches an ERROR event with the given error message.
     * @param {string} message Error message to include in the ERROR event.
     * @private
     */
    dispatchErrorEvent_: function(message) {
      var errorEvent = new cr.Event(CloudPrintInterface.EventType.ERROR);
      errorEvent.message = message;
      this.dispatchEvent(errorEvent);
    },

    /**
     * Called when the ready-state of a XML http request changes.
     * Calls the successCallback with the result or dispatches an ERROR event.
     * @param {XMLHttpRequest} xhr XML http request that changed.
     * @param {function(Object)} successCallback Callback to call if the request
     *     was successful.
     * @private
     */
    onReadyStateChange_: function(xhr, successCallback) {
      if (xhr.readyState == 4) {
        if (xhr.status == 200) {
          var result = JSON.parse(xhr.responseText);
          if (result['success']) {
            this.xsrfToken_ = result['xsrf_token'];
            successCallback(result);
          } else {
            this.dispatchErrorEvent_(result['message']);
          }
        } else {
          this.dispatchErrorEvent_(xhr.status + '');
        }
      }
    },

    /**
     * Called when the search request completes successfully.
     * @param {Object} result JSON response.
     * @private
     */
    onSearchDone_: function(result) {
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
      var isRecent = result['request']['params']['q'] == '^recent';
      var searchDoneEvent =
          new cr.Event(CloudPrintInterface.EventType.SEARCH_DONE);
      searchDoneEvent.printers = printerList;
      searchDoneEvent.isRecent = isRecent;
      searchDoneEvent.email = result['request']['user'];
      this.dispatchEvent(searchDoneEvent);
    },

    /**
     * Called when the submit request completes successfully.
     * @param {Object} result JSON response.
     * @private
     */
    onSubmitDone_: function(result) {
      var submitDoneEvent = new cr.Event(
          CloudPrintInterface.EventType.SUBMIT_DONE);
      submitDoneEvent.jobId = result['job']['id'];
      this.dispatchEvent(submitDoneEvent);
    },

    /**
     * Called when the printer request completes successfully.
     * @param {Object} result JSON response.
     * @private
     */
    onPrinterDone_: function(result) {
      // TODO(rltoscano): Better error handling here.
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
    },

    /**
     * Called when the update printer TOS acceptance request completes
     * successfully.
     * @param {Object} result JSON response.
     * @private
     */
    onUpdatePrinterTosAcceptanceDone_: function(result) {
      // Do nothing.
    }
  };

  // Export
  return {
    CloudPrintInterface: CloudPrintInterface
  };
});
