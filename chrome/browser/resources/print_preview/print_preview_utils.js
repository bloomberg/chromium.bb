// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} toTest The string to be tested.
 * @return {boolean} True if |toTest| contains only digits. Leading and trailing
 *     whitespace is allowed.
 */
function isInteger(toTest) {
  var numericExp = /^\s*[0-9]+\s*$/;
  return numericExp.test(toTest);
}

/**
 * Returns true if |value| is a valid non zero positive integer.
 * @param {string} value The string to be tested.
 *
 * @return {boolean} true if the |value| is valid non zero positive integer.
 */
function isPositiveInteger(value) {
  return isInteger(value) && parseInt(value, 10) > 0;
}

/**
 * Returns true if the contents of the two arrays are equal.
 * @param {Array} array1 The first array.
 * @param {Array} array2 The second array.
 *
 * @return {boolean} true if the arrays are equal.
 */
function areArraysEqual(array1, array2) {
  if (array1.length != array2.length)
    return false;
  for (var i = 0; i < array1.length; i++)
    if (array1[i] !== array2[i])
      return false;
  return true;
}

/**
 * Removes duplicate elements from |inArray| and returns a new array.
 * |inArray| is not affected. It assumes that |inArray| is already sorted.
 * @param {Array.<number>} inArray The array to be processed.
 * @return {Array.<number>} The array after processing.
 */
function removeDuplicates(inArray) {
  var out = [];

  if (inArray.length == 0)
    return out;

  out.push(inArray[0]);
  for (var i = 1; i < inArray.length; ++i)
    if (inArray[i] != inArray[i - 1])
      out.push(inArray[i]);
  return out;
}

/**
 * Checks if |pageRangeText| represents a valid page selection.
 * A valid selection has a parsable format and every page identifier is
 * <= |totalPageCount| unless wildcards are used (see examples).
 * Example: "1-4, 9, 3-6, 10, 11" is valid, assuming |totalPageCount| >= 11.
 * Example: "1-4, 6-6" is valid, assuming |totalPageCount| >= 6.
 * Example: "2-" is valid, assuming |totalPageCount| >= 2, means from 2 to the
 * end.
 * Example: "1-10000" is valid, regardless of |totalPageCount|, means from 1 to
 * the end if |totalPageCount| < 10000.
 * Example: "1-4dsf, 11" is invalid regardless of |totalPageCount|.
 * Example: "4-2, 11, -6" is invalid regardless of |totalPageCount|.
 *
 * Note: If |totalPageCount| is undefined the validation does not take
 * |totalPageCount| into account.
 * Example: "34853253" is valid.
 * Example: "1-4, 9, 3-6, 10, 11" is valid.
 *
 * @param {string} pageRangeText The text to be checked.
 * @param {number} totalPageCount The total number of pages.
 * @return {boolean} true if the |pageRangeText| is valid.
 */
function isPageRangeTextValid(pageRangeText, totalPageCount) {
  var regex = /^\s*([0-9]+)\s*-\s*([0-9]*)\s*$/;
  var successfullyParsed = 0;

  // Splitting around commas.
  var parts = pageRangeText.split(/,/);

  for (var i = 0; i < parts.length; ++i) {
    // Removing whitespace.
    parts[i] = parts[i].replace(/\s*/g, '');
    var match = parts[i].match(regex);
    if (parts[i].length == 0)
      continue;

    if (match && match[1] && isPositiveInteger(match[1])) {
      var from = parseInt(match[1], 10);
      var to = isPositiveInteger(match[2]) ? parseInt(match[2], 10) :
          totalPageCount;
      if (from > to || from > totalPageCount)
        return false;
    } else if (!isPositiveInteger(parts[i]) || (totalPageCount != -1 &&
          parseInt(parts[i], 10) > totalPageCount)) {
      return false;
    }
    successfullyParsed++;
  }
  return successfullyParsed > 0;
}

/**
 * Returns a list of all pages specified in |pagesRangeText|. The pages are
 * listed in the order they appear in |pageRangeText| and duplicates are not
 * eliminated. If |pageRangeText| is not valid according to
 * isPageRangeTextValid(), or |totalPageCount| is undefined an empty list is
 * returned.
 * @param {string} pageRangeText The text to be checked.
 * @param {number} totalPageCount The total number of pages.
 * @return {Array.<number>} A list of all pages.
 */
function pageRangeTextToPageList(pageRangeText, totalPageCount) {
  var pageList = [];
  if ((totalPageCount &&
       !isPageRangeTextValid(pageRangeText, totalPageCount)) ||
      !totalPageCount) {
    return pageList;
  }

  var regex = /^\s*([0-9]+)\s*-\s*([0-9]*)\s*$/;
  var parts = pageRangeText.split(/,/);

  for (var i = 0; i < parts.length; ++i) {
    var match = parts[i].match(regex);

    if (match && match[1]) {
      var from = parseInt(match[1], 10);
      var to = match[2] ? parseInt(match[2], 10) : totalPageCount;

      for (var j = from; j <= Math.min(to, totalPageCount); ++j)
        pageList.push(j);
    } else {
      var singlePageNumber = parseInt(parts[i], 10);
      if (isPositiveInteger(singlePageNumber.toString()) &&
          singlePageNumber <= totalPageCount) {
        pageList.push(singlePageNumber);
      }
    }
  }
  return pageList;
}

/**
 * @param {Array.<number>} pageList The list to be processed.
 * @return {Array.<number>} The contents of |pageList| in ascending order and
 *     without any duplicates. |pageList| is not affected.
 */
function pageListToPageSet(pageList) {
  var pageSet = [];
  if (pageList.length == 0)
    return pageSet;
  pageSet = pageList.slice(0);
  pageSet.sort(function(a, b) {
    return (/** @type {number} */ a) - (/** @type {number} */ b);
  });
  pageSet = removeDuplicates(pageSet);
  return pageSet;
}

/**
 * Converts |pageSet| to page ranges. It squashes whenever possible.
 * Example: '1-2,3,5-7' becomes 1-3,5-7.
 *
 * @param {Array.<number>} pageSet The set of pages to be processed. Callers
 *     should ensure that no duplicates exist.
 * @return {Array.<{from: number, to: number}>} An array of page range objects.
 */
function pageSetToPageRanges(pageSet) {
  var pageRanges = [];
  for (var i = 0; i < pageSet.length; ++i) {
    var tempFrom = pageSet[i];
    while (i + 1 < pageSet.length && pageSet[i + 1] == pageSet[i] + 1)
      ++i;
    var tempTo = pageSet[i];
    pageRanges.push({'from': tempFrom, 'to': tempTo});
  }
  return pageRanges;
}

/**
 * @param {!HTMLElement} element Element to check for visibility.
 * @return {boolean} Whether the given element is visible.
 */
function getIsVisible(element) {
  return !element.hidden;
}

/**
 * Shows or hides an element.
 * @param {!HTMLElement} element Element to show or hide.
 * @param {boolean} isVisible Whether the element should be visible or not.
 */
function setIsVisible(element, isVisible) {
  element.hidden = !isVisible;
}

/**
 * @param {!Array} array Array to check for item.
 * @param {*} item Item to look for in array.
 * @return {boolean} Whether the item is in the array.
 */
function arrayContains(array, item) {
  return array.indexOf(item) != -1;
}
