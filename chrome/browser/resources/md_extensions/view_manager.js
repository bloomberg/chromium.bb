// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * TODO(scottchen): shim for not having Animation.finished implemented. Can
   * replace with Animation.finished if Chrome implements it (see:
   * crbug.com/257235).
   * @param {!Animation} animation
   * @return {!Promise}
   */
  function whenFinished(animation) {
    return new Promise(function(resolve, reject) {
      animation.addEventListener('finish', resolve);
    });
  }

  /** @type {!Map<string, function(!Element): !Promise>} */
  const viewAnimations = new Map();
  viewAnimations.set('no-animation', () => Promise.resolve());
  viewAnimations.set('fade-in', element => {
    const animation = element.animate(
        {
          opacity: [0, 1],
        },
        /** @type {!KeyframeEffectOptions} */ ({
          duration: 180,
          easing: 'ease-in-out',
          iterations: 1,
        }));

    return whenFinished(animation);
  });
  viewAnimations.set('fade-out', element => {
    const animation = element.animate(
        {
          opacity: [1, 0],
        },
        /** @type {!KeyframeEffectOptions} */ ({
          duration: 180,
          easing: 'ease-in-out',
          iterations: 1,
        }));

    return whenFinished(animation);
  });

  const ViewManager = Polymer({
    is: 'extensions-view-manager',

    /**
     * @param {!Element} element
     * @param {string} animation
     * @return {!Promise}
     * @private
     */
    exit_: function(element, animation) {
      const animationFunction = extensions.viewAnimations.get(animation);
      assert(animationFunction);

      element.classList.remove('active');
      element.classList.add('closing');
      element.dispatchEvent(new CustomEvent('view-exit-start'));
      return animationFunction(element).then(function() {
        element.classList.remove('closing');
        element.dispatchEvent(new CustomEvent('view-exit-finish'));
      });
    },

    /**
     * @param {!Element} element
     * @param {!string} animation
     * @return {!Promise}
     * @private
     */
    enter_: function(element, animation) {
      const animationFunction = extensions.viewAnimations.get(animation);
      assert(animationFunction);

      element.classList.add('active');
      element.dispatchEvent(new CustomEvent('view-enter-start'));
      return animationFunction(element).then(function() {
        element.dispatchEvent(new CustomEvent('view-enter-finish'));
      });
    },

    /**
     * @param {string} newViewId
     * @param {string=} enterAnimation
     * @param {string=} exitAnimation
     * @return {!Promise}
     */
    switchView: function(newViewId, enterAnimation, exitAnimation) {
      const previousView = this.querySelector('.active');
      const newView = assert(this.querySelector('#' + newViewId));

      const promises = [];
      if (previousView) {
        promises.push(this.exit_(previousView, exitAnimation || 'fade-out'));
        promises.push(this.enter_(newView, enterAnimation || 'fade-in'));
      } else {
        promises.push(this.enter_(newView, 'no-animation'));
      }

      return Promise.all(promises);
    },

    /**
     * Helper function to only animate the current view.
     * @param {string} animation
     * @return {!Promise}
     */
    animateCurrentView: function(animation) {
      const currentView = assert(this.querySelector('.active'));
      const animationFunction = extensions.viewAnimations.get(animation);
      assert(animationFunction);

      return animationFunction(currentView);
    },
  });

  return {viewAnimations: viewAnimations, ViewManager: ViewManager};
});