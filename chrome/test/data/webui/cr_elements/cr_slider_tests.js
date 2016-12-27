// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-slider. */
cr.define('cr_slider', function() {
  function registerTests() {
    suite('cr-slider', function() {
      /** @type {!CrSliderElement} */
      var slider;

      /**
       * paper-slider instance wrapped by cr-slider.
       * @type {!PaperSliderElement}
       */
      var paperSlider;

      var tickValues = [2, 4, 8, 16, 32, 64, 128];

      setup(function() {
        PolymerTest.clearBody();
        slider = document.createElement('cr-slider');
        document.body.appendChild(slider);
        paperSlider = slider.$$('paper-slider');
      });

      test('set value', function() {
        slider.tickValues = tickValues;
        slider.value = 16;
        expectEquals(6, paperSlider.max);
        expectEquals(3, paperSlider.value);
        expectEquals(3, paperSlider.immediateValue);

        // cr-slider only supports snapping to a range of tick values. Setting
        // to an in-between value should snap to an indexed value.
        slider.value = 70;
        expectEquals(5, paperSlider.value);
        expectEquals(5, paperSlider.immediateValue);
        expectEquals(64, slider.value);

        // Setting the value out-of-range should clamp the slider.
        slider.value = -100;
        expectEquals(0, paperSlider.value);
        expectEquals(0, paperSlider.immediateValue);
        expectEquals(2, slider.value);
      });

      test('move slider', function() {
        slider.tickValues = tickValues;
        slider.value = 30;
        expectEquals(4, paperSlider.value);

        MockInteractions.pressAndReleaseKeyOn(
            paperSlider, 39 /* right */);
        expectEquals(5, paperSlider.value);
        expectEquals(64, slider.value);

        MockInteractions.pressAndReleaseKeyOn(
            paperSlider, 39 /* right */);
        expectEquals(6, paperSlider.value);
        expectEquals(128, slider.value);

        MockInteractions.pressAndReleaseKeyOn(
            paperSlider, 39 /* right */);
        expectEquals(6, paperSlider.value);
        expectEquals(128, slider.value);

        MockInteractions.pressAndReleaseKeyOn(
            paperSlider, 37 /* left */);
        expectEquals(5, paperSlider.value);
        expectEquals(64, slider.value);
      });

      test('findNearestIndex_', function() {
        var slider = document.createElement('cr-slider');
        var testArray = [80, 20, 350, 1000, 200, 100];
        var testFindNearestIndex = function(expectedIndex, value) {
          expectEquals(
              expectedIndex, slider.findNearestIndex_(testArray, value));
        };
        testFindNearestIndex(0, 51);
        testFindNearestIndex(0, 80);
        testFindNearestIndex(0, 89);
        testFindNearestIndex(1, -100);
        testFindNearestIndex(1, 20);
        testFindNearestIndex(1, 49);
        testFindNearestIndex(2, 400);
        testFindNearestIndex(2, 350);
        testFindNearestIndex(2, 300);
        testFindNearestIndex(3, 200000);
        testFindNearestIndex(3, 1000);
        testFindNearestIndex(3, 700);
        testFindNearestIndex(4, 220);
        testFindNearestIndex(4, 200);
        testFindNearestIndex(4, 151);
        testFindNearestIndex(5, 149);
        testFindNearestIndex(5, 100);
        testFindNearestIndex(5, 91);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
