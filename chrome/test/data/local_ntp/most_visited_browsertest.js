// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Tests the Most Visited iframe on the local NTP.
 */


/**
 * Most Visited's object for test and setup functions.
 */
test.mostVisited = {};


/**
 * The MostVisited object.
 * @type {?Object}
 */
test.mostVisited.mostvisited = null;


/**
 * The MostVisited grid.
 * @type {?Grid}
 */
test.mostVisited.grid = null;


/**
 * Set up the text DOM and test environment.
 */
test.mostVisited.setUp = function() {
  setUpPage('most-visited-template');

  test.mostVisited.mostvisited = test.mostVisited.init();
  test.mostVisited.grid = new test.mostVisited.mostvisited.Grid();
  document.documentElement.dir = null;
};


/**
 * Tests if the grid creates the correct tile layout.
 */
test.mostVisited.testGridLayout = function() {
  window.innerWidth = 100;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 6,
    maxTilesPerRow: 5,
    maxTiles: 10
  };

  // Create a grid with 1 tile.
  let container = document.createElement('div');
  let expectedLayout = ['translate(0px, 0px)'];
  test.mostVisited.initGrid(container, params, 1);
  assertEquals('10px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with a full row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(30px, 0px)', 'translate(40px, 0px)'
  ];
  test.mostVisited.initGrid(container, params, 5);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with balanced rows. There should be 3 tiles per row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(0px, 10px)', 'translate(10px, 10px)', 'translate(20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with two uneven rows. The second row should be offset by a
  // half tile.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(30px, 0px)', 'translate(5px, 10px)', 'translate(15px, 10px)',
    'translate(25px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 7);
  assertEquals('40px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with max rows.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(30px, 0px)', 'translate(40px, 0px)', 'translate(0px, 10px)',
    'translate(10px, 10px)', 'translate(20px, 10px)', 'translate(30px, 10px)',
    'translate(40px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 10);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
};


/**
 * Tests if the grid creates the correct tile layout in RTL. The (x,y) positions
 * should be the same as LTR except with a negative x value.
 */
test.mostVisited.testGridLayoutRtl = function() {
  document.documentElement.dir = 'rtl';  // Enable RTL.
  window.innerWidth = 100;
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 6,
    maxTilesPerRow: 5,
    maxTiles: 10
  };

  // Create a grid with 1 tile.
  let container = document.createElement('div');
  let expectedLayout = ['translate(0px, 0px)'];
  test.mostVisited.initGrid(container, params, 1);
  assertEquals('10px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with a full row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(-30px, 0px)', 'translate(-40px, 0px)'
  ];
  test.mostVisited.initGrid(container, params, 5);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with balanced rows. There should be 3 tiles per row.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(0px, 10px)', 'translate(-10px, 10px)', 'translate(-20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with two uneven rows. The second row should be offset by a
  // half tile.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(-30px, 0px)', 'translate(-5px, 10px)', 'translate(-15px, 10px)',
    'translate(-25px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 7);
  assertEquals('40px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);

  // Create a grid with max rows.
  container = document.createElement('div');
  expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(-30px, 0px)', 'translate(-40px, 0px)', 'translate(0px, 10px)',
    'translate(-10px, 10px)', 'translate(-20px, 10px)',
    'translate(-30px, 10px)', 'translate(-40px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 10);
  assertEquals('50px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
};


/**
 * Tests if the grid resizes correctly according to window size.
 */
test.mostVisited.testGridResize = function() {
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 4,
    maxTilesPerRow: 3,
    maxTiles: 6
  };

  window.innerWidth = 30;
  // Create a grid with max rows.
  let container = document.createElement('div');
  let expectedLayout = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(20px, 0px)',
    'translate(0px, 10px)', 'translate(10px, 10px)', 'translate(20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);

  // Shrink the window so that only 4 tiles are visible.
  window.innerWidth = 20;
  // The last two tiles will still increment their x coordinate, but they will
  // be hidden.
  let expectedLayoutShrink = [
    'translate(0px, 0px)', 'translate(10px, 0px)', 'translate(0px, 10px)',
    'translate(10px, 10px)', 'translate(20px, 10px)', 'translate(30px, 10px)'
  ];
  test.mostVisited.grid.onResize();
  assertEquals('20px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayoutShrink);
  test.mostVisited.assertVisibility(container, 2);

  // Expand the window so that all tiles are visible.
  window.innerWidth = 100;
  test.mostVisited.grid.onResize();
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);
};


/**
 * Tests if the grid resizes correctly in RTL according to window size. The
 * (x,y) positions should be the same as LTR except with a negative x value.
 */
test.mostVisited.testGridResizeRtl = function() {
  document.documentElement.dir = 'rtl';  // Enable RTL.
  const params = {  // Used to override the default grid parameters.
    tileHeight: 10,
    tileWidth: 10,
    tilesAlwaysVisible: 4,
    maxTilesPerRow: 3,
    maxTiles: 6
  };

  window.innerWidth = 30;
  // Create a grid with max rows.
  let container = document.createElement('div');
  let expectedLayout = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(-20px, 0px)',
    'translate(0px, 10px)', 'translate(-10px, 10px)', 'translate(-20px, 10px)'
  ];
  test.mostVisited.initGrid(container, params, 6);
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);

  // Shrink the window so that only 4 tiles are visible.
  window.innerWidth = 20;
  // The last two tiles will still increment their x coordinate, but they will
  // be hidden.
  let expectedLayoutShrink = [
    'translate(0px, 0px)', 'translate(-10px, 0px)', 'translate(0px, 10px)',
    'translate(-10px, 10px)', 'translate(-20px, 10px)', 'translate(-30px, 10px)'
  ];
  test.mostVisited.grid.onResize();
  assertEquals('20px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayoutShrink);
  test.mostVisited.assertVisibility(container, 2);

  // Expand the window so that all tiles are visible.
  window.innerWidth = 100;
  test.mostVisited.grid.onResize();
  assertEquals('30px', container.style.width);
  test.mostVisited.assertLayout(container, expectedLayout);
  test.mostVisited.assertVisibility(container, 0);
};


// ***************************** HELPER FUNCTIONS *****************************
// These are used by the tests above.


/**
 * Creates and initializes a MostVisited object.
 * @return {!Object} The MostVisited object.
 */
test.mostVisited.init = function() {
  const mostVisited = MostVisited();
  mostVisited.init();
  return mostVisited;
};


/**
 * Fills and initializes the tile grid.
 * @param {!Element} container The grid container.
 * @param {!Object} params Overrides for the default grid parameters.
 * @param {number} numTiles The number of tiles to fill the grid with.
 */
test.mostVisited.initGrid = function(container, params, numTiles) {
  assertTrue(!!test.mostVisited.grid);
  test.mostVisited.fillTiles(container, numTiles);
  test.mostVisited.grid.init(container, params);
};


/**
 * Fills |container| with |numTiles| new grid tiles.
 * @param {!Element} container The grid container.
 * @param {number} numTiles Number of tiles to create.
 */
test.mostVisited.fillTiles = function(container, numTiles) {
  assertTrue(!!test.mostVisited.grid);
  for (let i = 0; i < numTiles; i++) {
    const tile = document.createElement('div');
    const gridTile = test.mostVisited.grid.createGridTile(tile);
    container.appendChild(gridTile);
  }
};


/**
 * Assert that the tile layout matches |expectedLayout|.
 * @param {!Element} container The grid container.
 * @param {!Array<string>} expectedLayout The transform values for each tile.
 */
test.mostVisited.assertLayout = function(container, expectedLayout) {
  const tiles = container.children;
  assertEquals(expectedLayout.length, tiles.length);
  for (let i = 0; i < tiles.length; i++) {
    assertEquals(expectedLayout[i], tiles[i].style.transform);
  }
};


/**
 * Assert that all but the last |numHidden| tiles are visible.
 * @param {!Element} container The grid container.
 * @param {number} numHidden The number of hidden tiles.
 */
test.mostVisited.assertVisibility = function(container, numHidden) {
  const tiles = container.children;
  for (let i = 0; i < tiles.length; i++) {
    if (i < tiles.length - numHidden) {
      assertFalse(tiles[i].style.display === 'none');
    } else {
      assertTrue(tiles[i].style.display === 'none');
    }
  }
};
