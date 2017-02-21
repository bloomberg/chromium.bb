// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var scene = {};

/**
 * The scene class assists in managing element and animations in the UI.  It
 * allows UI update API commands to be queued in batches, and manages allocation
 * of element and animation IDs.
 *
 * Examples:
 *
 * var ui = new scene.Scene();
 *
 * // Add an element.
 * var el = new api.UiElement(100, 200, 50, 50);
 * el.setSize(buttonWidth, buttonHeight);
 *
 * // Anchor it to the bottom of the content quad.
 * el.setParentId(contentQuadId);
 * el.setAnchoring(api.XAnchoring.XNONE, api.YAnchoring.YBOTTOM);
 *
 * // Place it just below the content quad edge.
 * el.setTranslation(0, -0.2, 0.0);
 *
 * // Add it to the ui.
 * var buttonId = ui.addElement(el);
 * ui.flush();
 *
 * // Make the button twice as big.
 * var update = new api.UiElementUpdate();
 * update.setSize(bunttonWidth * 2, buttonHeight * 2);
 * ui.updateElement(buttonId, update);
 * ui.flush();
 *
 * // Animate the button size back to its original size, over 250 ms.
 * var resize = new api.Animation(buttonId, 250);
 * resize.setSize(buttonWidth, buttonHeight);
 * ui.addAnimation(resize);
 * ui.flush();
 *
 * @struct
 */
scene.Scene = class {
  constructor() {
    /** @private {number} */
    this.idIndex = 1;
    /** @private {Array<Object>} */
    this.commands = [];
    /** @private {!Set<number>} */
    this.elements = new Set();
    /** @private {!Object} */
    this.animations = [];
  }

  /**
   * Flush all queued commands to native.
   */
  flush() {
    api.sendCommands(this.commands);
    this.commands = [];
  }

  /**
   * Add a new UiElementUpdate to the scene, returning the ID assigned.
   * @param {api.UiElementUpdate} element
   */
  addElement(element) {
    var id = this.idIndex++;
    element.setId(id);
    this.commands.push(
        {'type': api.Command.ADD_ELEMENT, 'data': element.properties});
    this.elements.add(id);
    return id;
  }

  /**
   * Update an existing element, according to a UiElementUpdate object.
   * @param {number} id
   * @param {api.UiElementUpdate} update
   */
  updateElement(id, update) {
    // To-do:  Make sure ID exists.
    update.setId(id);
    this.commands.push(
        {'type': api.Command.UPDATE_ELEMENT, 'data': update.properties});
  }

  /**
   * Remove an element from the scene.
   * @param {number} id
   */
  removeElement(id) {
    // To-do: Make sure ID exists.
    this.commands.push(
        {'type': api.Command.REMOVE_ELEMENT, 'data': {'id': id}});
    this.elements.delete(id);
  }

  /**
   * Add a new Animation to the scene, returning the ID assigned.
   * @param {api.Animation} animation
   */
  addAnimation(animation) {
    var id = this.idIndex++;
    animation.setId(id);
    this.commands.push({'type': api.Command.ADD_ANIMATION, 'data': animation});
    this.animations[id] = animation.meshId;
    return id;
  }

  /**
   * Remove an animation from the scene.
   *
   * Note that animations are flushed when they complete and are not required
   * to be removed. Also new animations of the same type will effectively
   * override the original so there is no need to remove in that scenario
   * either.
   *
   * @param {number} id
   */
  removeAnimation(id) {
    // To-do: Make sure ID exists.
    this.commands.push({
      'type': api.Command.REMOVE_ANIMATION,
      'data': {'id': id, 'meshId': this.animations[id]}
    });
    delete this.animations[id];
  }

  /**
   * Set the background color of the scene.
   * @param {{r: number, b: number, g: number, a: number}} color
   */
  setBackgroundColor(color) {
    this.commands.push(
        {'type': api.Command.UPDATE_BACKGROUND, 'data': {'color': color}});
  }

  /**
   * Set the radius of background-bounding sphere.
   * @param {number} distance
   */
  setBackgroundDistance(distance) {
    this.commands.push({
      'type': api.Command.UPDATE_BACKGROUND,
      'data': {'distance': distance}
    });
  }

  /**
   * Purge all elements in the scene.
   */
  purge() {
    var ids = Object.keys(this.animations);
    for (let id_key of ids) {
      var id = parseInt(id_key, 10);
      this.removeAnimation(id);
    }
    var ids = this.elements.values();
    for (let id of ids) {
      this.removeElement(id);
    }
    this.flush();
  }
};
