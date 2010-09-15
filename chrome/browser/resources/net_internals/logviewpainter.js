// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(eroman): This needs better presentation, and cleaner code. This
 *               implementation is more of a transitionary step as
 *               the old net-internals is replaced.
 */

// TODO(eroman): these functions should use lower-case names.
var PaintLogView;
var PrintSourceEntriesAsText;
var proxySettingsToString;

// Start of anonymous namespace.
(function() {

PaintLogView = function(sourceEntries, node) {
  for (var i = 0; i < sourceEntries.length; ++i) {
    if (i != 0)
      addNode(node, 'hr');
    addSourceEntry_(node, sourceEntries[i]);
  }
}

const INDENTATION_PX = 20;

function addSourceEntry_(node, sourceEntry) {
  var div = addNode(node, 'div');
  div.className = 'logSourceEntry';

  var p = addNode(div, 'p');
  var nobr = addNode(p, 'nobr');

  addTextNode(nobr, sourceEntry.getDescription());

  var p2 = addNode(div, 'p');
  var nobr2 = addNode(p2, 'nobr');

  var logEntries = sourceEntry.getLogEntries();
  var startDate = g_browser.convertTimeTicksToDate(logEntries[0].time);
  addTextNode(nobr2, 'Start Time: ' + startDate.toLocaleString());

  var pre = addNode(div, 'pre');
  addTextNode(pre, PrintSourceEntriesAsText(logEntries, false));
}

function canCollapseBeginWithEnd(beginEntry) {
  return beginEntry &&
         beginEntry.isBegin() &&
         !beginEntry.orig.params &&
         beginEntry.end &&
         beginEntry.end.index == beginEntry.index + 1 &&
         !beginEntry.end.orig.params &&
         beginEntry.orig.wasPassivelyCaptured ==
             beginEntry.end.orig.wasPassivelyCaptured;
}

PrintSourceEntriesAsText = function(sourceEntries, doSecurityStripping) {
  var entries = LogGroupEntry.createArrayFrom(sourceEntries);
  if (entries.length == 0)
    return '';

  var startDate = g_browser.convertTimeTicksToDate(entries[0].orig.time);
  var startTime = startDate.getTime();

  var tablePrinter = new TablePrinter();

  for (var i = 0; i < entries.length; ++i) {
    var entry = entries[i];

    // Avoid printing the END for a BEGIN that was immediately before.
    if (entry.isEnd() && canCollapseBeginWithEnd(entry.begin))
      continue;

    tablePrinter.addRow();

    // Annotate this entry with "(P)" if it was passively captured.
    tablePrinter.addCell(entry.orig.wasPassivelyCaptured ? '(P) ' : '');

    tablePrinter.addCell('t=');
    var date = g_browser.convertTimeTicksToDate(entry.orig.time) ;
    var tCell = tablePrinter.addCell(date.getTime());
    tCell.alignRight = true;
    tablePrinter.addCell(' [st=');
    var stCell = tablePrinter.addCell(date.getTime() - startTime);
    stCell.alignRight = true;
    tablePrinter.addCell('] ');

    var indentationStr = makeRepeatedString(' ', entry.getDepth() * 3);
    var mainCell =
        tablePrinter.addCell(indentationStr + getTextForEvent(entry));
    tablePrinter.addCell('  ');

    // Get the elapsed time.
    if (entry.isBegin()) {
      tablePrinter.addCell('[dt=');
      var dt = '?';
      // Definite time.
      if (entry.end) {
        dt = entry.end.orig.time - entry.orig.time;
      }
      var dtCell = tablePrinter.addCell(dt);
      dtCell.alignRight = true;

      tablePrinter.addCell(']');
    } else {
      mainCell.allowOverflow = true;
    }

    // Output the extra parameters.
    if (entry.orig.params != undefined) {
      // Add a continuation row for each line of text from the extra parameters.
      var extraParamsText = getTextForExtraParams(entry.orig,
                                                  doSecurityStripping);
      var extraParamsTextLines = extraParamsText.split('\n');

      for (var j = 0; j < extraParamsTextLines.length; ++j) {
        tablePrinter.addRow();
        tablePrinter.addCell('');  // Empty passive annotation.
        tablePrinter.addCell('');  // No t=.
        tablePrinter.addCell('');
        tablePrinter.addCell('');  // No st=.
        tablePrinter.addCell('');
        tablePrinter.addCell('  ');

        var mainExtraCell =
            tablePrinter.addCell(indentationStr + extraParamsTextLines[j]);
        mainExtraCell.allowOverflow = true;
      }
    }
  }

  // Format the table for fixed-width text.
  return tablePrinter.toText();
}

function getTextForExtraParams(entry, doSecurityStripping) {
  // Format the extra parameters (use a custom formatter for certain types,
  // but default to displaying as JSON).
  switch (entry.type) {
    case LogEventType.HTTP_TRANSACTION_SEND_REQUEST_HEADERS:
    case LogEventType.HTTP_TRANSACTION_SEND_TUNNEL_HEADERS:
      return getTextForRequestHeadersExtraParam(entry, doSecurityStripping);

    case LogEventType.HTTP_TRANSACTION_READ_RESPONSE_HEADERS:
    case LogEventType.HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS:
      return getTextForResponseHeadersExtraParam(entry, doSecurityStripping);

    case LogEventType.PROXY_CONFIG_CHANGED:
      return getTextForProxyConfigChangedExtraParam(entry);

    default:
      var out = [];
      for (var k in entry.params) {
        if (k == 'headers' && entry.params[k] instanceof Array) {
          out.push(getTextForResponseHeadersExtraParam(entry,
                                                       doSecurityStripping));
          continue;
        }
        var value = entry.params[k];
        var paramStr = ' --> ' + k + ' = ' + JSON.stringify(value);

        // Append the symbolic name for certain constants. (This relies
        // on particular naming of event parameters to infer the type).
        if (typeof value == 'number') {
          if (k == 'net_error') {
            paramStr += ' (' + getNetErrorSymbolicString(value) + ')';
          } else if (k == 'load_flags') {
            paramStr += ' (' + getLoadFlagSymbolicString(value) + ')';
          }
        }

        out.push(paramStr);
      }
      return out.join('\n');
  }
}

/**
 * Returns the name for netError.
 *
 * Example: getNetErrorSymbolicString(-105) would return
 * "NAME_NOT_RESOLVED".
 */
function getNetErrorSymbolicString(netError) {
  return getKeyWithValue(NetError, netError);
}

/**
 * Returns the set of LoadFlags that make up the integer |loadFlag|.
 * For example: getLoadFlagSymbolicString(
 */
function getLoadFlagSymbolicString(loadFlag) {
  // Load flag of 0 means "NORMAL". Special case this, since and-ing with
  // 0 is always going to be false.
  if (loadFlag == 0)
    return getKeyWithValue(LoadFlag, loadFlag);

  var matchingLoadFlagNames = [];

  for (var k in LoadFlag) {
    if (loadFlag & LoadFlag[k])
      matchingLoadFlagNames.push(k);
  }

  return matchingLoadFlagNames.join(' | ');
}

/**
 * Indent |lines| by |start|.
 *
 * For example, if |start| = ' -> ' and |lines| = ['line1', 'line2', 'line3']
 * the output will be:
 *
 *   " -> line1\n" +
 *   "    line2\n" +
 *   "    line3"
 */
function indentLines(start, lines) {
  return start + lines.join('\n' + makeRepeatedString(' ', start.length));
}

/**
 * Removes a cookie or unencrypted login information from a single HTTP header
 * line, if present, and returns the modified line.  Otherwise, just returns
 * the original line.
 */
function stripCookieOrLoginInfo(line) {
  var patterns = [
      // Cookie patterns
      /^set-cookie:/i,
      /^set-cookie2:/i,
      /^cookie:/i,

      // Unencrypted authentication patterns
      /^authorization: \S*/i,
      /^proxy-authorization: \S*/i];

  for (var i = 0; i < patterns.length; i++) {
    var match = patterns[i].exec(line);
    if (match != null)
      return match + " [value was stripped]";
  }
  return line;
}

/**
 * Removes all cookie and unencrypted login text from a list of HTTP
 * header lines.
 */
function stripCookiesAndLoginInfo(headers) {
  return headers.map(stripCookieOrLoginInfo);
}

function getTextForRequestHeadersExtraParam(entry, doSecurityStripping) {
  var params = entry.params;

  // Strip the trailing CRLF that params.line contains.
  var lineWithoutCRLF = params.line.replace(/\r\n$/g, '');

  var headers = params.headers;
  if (doSecurityStripping)
    headers = stripCookiesAndLoginInfo(headers);

  return indentLines(' --> ', [lineWithoutCRLF].concat(headers));
}

function getTextForResponseHeadersExtraParam(entry, doSecurityStripping) {
  var headers = entry.params.headers;
  if (doSecurityStripping)
    headers = stripCookiesAndLoginInfo(headers);
  return indentLines(' --> ', headers);
}

function getTextForProxyConfigChangedExtraParam(entry) {
  var params = entry.params;
  var out = '';
  var indentation = '        ';

  if (params.old_config) {
    var oldConfigString = proxySettingsToString(params.old_config);
    // The previous configuration may not be present in the case of
    // the initial proxy settings fetch.
    out += ' --> old_config =\n' +
           indentLines(indentation, oldConfigString.split('\n'));
    out += '\n';
  }

  var newConfigString = proxySettingsToString(params.new_config);
  out += ' --> new_config =\n' +
         indentLines(indentation, newConfigString.split('\n'));

  return out;
}

function getTextForEvent(entry) {
  var text = '';

  if (entry.isBegin() && canCollapseBeginWithEnd(entry)) {
    // Don't prefix with '+' if we are going to collapse the END event.
    text = ' ';
  } else if (entry.isBegin()) {
    text = '+' + text;
  } else if (entry.isEnd()) {
    text = '-' + text;
  } else {
    text = ' ';
  }

  text += getKeyWithValue(LogEventType, entry.orig.type);
  return text;
}

proxySettingsToString = function(config) {
  if (!config)
    return '';

  // The proxy settings specify up to three major fallback choices
  // (auto-detect, custom pac url, or manual settings).
  // We enumerate these to a list so we can later number them.
  var modes = [];

  // Output any automatic settings.
  if (config.auto_detect)
    modes.push(['Auto-detect']);
  if (config.pac_url)
    modes.push(['PAC script: ' + config.pac_url]);

  // Output any manual settings.
  if (config.single_proxy || config.proxy_per_scheme) {
    var lines = [];

    if (config.single_proxy) {
      lines.push('Proxy server: ' + config.single_proxy);
    } else if (config.proxy_per_scheme) {
      for (var urlScheme in config.proxy_per_scheme) {
        if (urlScheme != 'fallback') {
          lines.push('Proxy server for ' + urlScheme.toUpperCase() + ': ' +
                     config.proxy_per_scheme[urlScheme]);
        }
      }
      if (config.proxy_per_scheme.fallback) {
        lines.push('Proxy server for everything else: ' +
                   config.proxy_per_scheme.fallback);
      }
    }

    // Output any proxy bypass rules.
    if (config.bypass_list) {
      if (config.reverse_bypass) {
        lines.push('Reversed bypass list: ');
      } else {
        lines.push('Bypass list: ');
      }

      for (var i = 0; i < config.bypass_list.length; ++i)
        lines.push('  ' + config.bypass_list[i]);
    }

    modes.push(lines);
  }

  // If we didn't find any proxy settings modes, we are using DIRECT.
  if (modes.length < 1)
    return 'Use DIRECT connections.';

  // If there was just one mode, don't bother numbering it.
  if (modes.length == 1)
    return modes[0].join('\n');

  // Otherwise concatenate all of the modes into a numbered list
  // (which correspond with the fallback order).
  var result = [];
  for (var i = 0; i < modes.length; ++i)
    result.push(indentLines('(' + (i + 1) + ') ', modes[i]));

  return result.join('\n');
};

// End of anonymous namespace.
})();

