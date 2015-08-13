// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Parser for a simple grammar that describes a tree structure using a function-
 * like "a(b(c,d))" syntax. Original intended usage: to have browsertests
 * specify an arbitrary tree of iframes, loaded from various sites, without
 * having to write a .html page for each level or do crazy feats of data: url
 * escaping. But there's nothing really iframe-specific here. See below for some
 * examples of the grammar and the parser output.
 *
 * @example <caption>Basic syntax: an identifier followed by arg list.</caption>
 * TreeParserUtil.parse('abc ()');  // returns { value: 'abc', children: [] }
 *
 * @example <caption>The arg list is optional. Dots are legal in ids.</caption>
 * TreeParserUtil.parse('b.com');  // returns { value: 'b.com', children: [] }
 *
 * @example <caption>Commas separate children in the arg list.</caption>
 * // returns { value: 'b', children: [
 * //           { value: 'c', children: [] },
 * //           { value: 'd', children: [] }
 * //         ]}
 * TreeParserUtil.parse('b (c, d)';
 *
 * @example <caption>Children can have children, and so on.</caption>
 * // returns { value: 'e', children: [
 * //           { value: 'f', children: [
 * //             { value: 'g', children: [
 * //               { value: 'h', children: [] },
 * //               { value: 'i', children: [
 * //                 { value: 'j', children: [] }
 * //               ]},
 * //             ]}
 * //           ]}
 * //         ]}
 * TreeParserUtil.parse('e(f(g(h(),i(j))))';
 *
 * @example <caption>flatten() converts a [sub]tree back to a string.</caption>
 * var tree = TreeParserUtil.parse('b.com (c.com(e.com), d.com)');
 * TreeParserUtil.flatten(tree.children[0]);  // returns 'c.com(e.com())'
 */
var TreeParserUtil = (function() {
  'use strict';

  /**
   * Parses an input string into a tree. See class comment for examples.
   * @returns A tree of the form {value: <string>, children: <Array.<tree>>}.
   */
  function parse(input) {
    var tokenStream = lex(input);

    var result = takeIdAndChild(tokenStream);
    if (tokenStream.length != 0)
      throw new Error('Expected end of stream, but found "' +
                      tokenStream[0] + '".')
    return result;
  }

  /**
   * Inverse of |parse|. Converts a parsed tree object into a string. Can be
   * used to forward a subtree as an argument to a nested document.
   */
  function flatten(tree) {
    return tree.value + '(' + tree.children.map(flatten).join(',') + ')';
  }

  /**
   * Lexer function to convert an input string into a token stream.  Splits the
   * input along whitespace, parens and commas. Whitespace is removed, while
   * parens and commas are preserved as standalone tokens.
   *
   * @param {string} input The input string.
   * @return {Array.<string>} The resulting token stream.
   */
  function lex(input) {
    return input.split(/(\s+|\(|\)|,)/).reduce(
      function (resultArray, token) {
        var trimmed = token.trim();
        if (trimmed) {
          resultArray.push(trimmed);
        }
        return resultArray;
      }, []);
  }


  /**
   * Consumes from the stream an identifier and optional child list, returning
   * its parsed representation.
   */
  function takeIdAndChild(tokenStream) {
    return { value: takeIdentifier(tokenStream),
             children: takeChildList(tokenStream) };
  }

  /**
   * Consumes from the stream an identifier, returning its value (as a string).
   */
  function takeIdentifier(tokenStream) {
    if (tokenStream.length == 0)
      throw new Error('Expected an identifier, but found end-of-stream.');
    var token = tokenStream.shift();
    if (!token.match(/[a-zA-Z0-9.-]+/))
      throw new Error('Expected an identifier, but found "' + token + '".');
    return token;
  }

  /**
   * Consumes an optional child list from the token stream, returning a list of
   * the parsed children.
   */
  function takeChildList(tokenStream) {
    // Remove the next token from the stream if it matches |token|.
    function tryToEatA(token) {
      if (tokenStream[0] == token) {
        tokenStream.shift();
        return true;
      }
      return false;
    }

    // Bare identifier case, as in 'b' in the input '(a (b, c))'
    if (!tryToEatA('('))
      return [];

    // Empty list case, as in 'b' in the input 'a (b (), c)'.
    if (tryToEatA(')')) {
      return [];
    }

    // List with at least one entry.
    var result = [ takeIdAndChild(tokenStream) ];

    // Additional entries allowed with commas.
    while (tryToEatA(',')) {
      result.push(takeIdAndChild(tokenStream));
    }

    // End of list.
    if (tryToEatA(')')) {
      return result;
    }
    if (tokenStream.length == 0)
      throw new Error('Expected ")" or ",", but found end-of-stream.');
    throw new Error('Expected ")" or ",", but found "' + tokenStream[0] + '".');
  }

  return {
    parse:   parse,
    flatten: flatten
  };
})();
