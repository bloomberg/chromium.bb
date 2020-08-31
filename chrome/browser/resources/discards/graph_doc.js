// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Target y position for page nodes.
const kPageNodesTargetY = 20;

// Range occupied by page nodes at the top of the graph view.
const kPageNodesYRange = 100;

// Range occupied by process nodes at the bottom of the graph view.
const kProcessNodesYRange = 100;

// Range occupied by worker nodes at the bottom of the graph view, above
// process nodes.
const kWorkerNodesYRange = 200;

// Target y position for frame nodes.
const kFrameNodesTargetY = kPageNodesYRange + 50;

// Range that frame nodes cannot enter at the top/bottom of the graph view.
const kFrameNodesTopMargin = kPageNodesYRange;
const kFrameNodesBottomMargin = kWorkerNodesYRange + 50;

class ToolTip {
  /**
   * @param {Element} div
   * @param {GraphNode} node
   */
  constructor(div, node) {
    /** @type {boolean} */
    this.floating = true;

    /** @type {number} */
    this.x = node.x;

    /** @type {number} */
    this.y = node.y - 28;

    /** @type {GraphNode} */
    this.node = node;

    /** @private {d3.selection} */
    this.div_ = d3.select(div)
                    .append('div')
                    .attr('class', 'tooltip')
                    .style('opacity', 0)
                    .style('left', `${this.x}px`)
                    .style('top', `${this.y}px`);
    this.div_.append('table').append('tbody');
    this.div_.transition().duration(200).style('opacity', .9);

    /** @private {string} */
    this.description_json_ = '';
    // Set up a drag behavior for this object's div.
    const drag = d3.drag().subject(() => this);
    drag.on('start', this.onDragStart_.bind(this));
    drag.on('drag', this.onDrag_.bind(this));
    this.div_.call(drag);

    this.onDescription(JSON.stringify({}));
  }

  nodeMoved() {
    if (!this.floating) {
      return;
    }

    const node = this.node;
    this.x = node.x;
    this.y = node.y - 28;
    this.div_.style('left', `${this.x}px`).style('top', `${this.y}px`);
  }

  /**
   * @return {!Array<number>} The [x, y] center of the ToolTip's div
   * element.
   */
  getCenter() {
    const rect = this.div_.node().getBoundingClientRect();
    return [rect.x + rect.width / 2, rect.y + rect.height / 2];
  }

  goAway() {
    this.div_.transition().duration(200).style('opacity', 0).remove();
  }

  /**
   * Updates the description displayed.
   * @param {string} description_json A JSON string.
   */
  onDescription(description_json) {
    if (this.description_json_ === description_json) {
      return;
    }

    /**
     * Helper for recursively flattening an Object.
     *
     * @param {!Set} visited The set of visited objects, excluding
     *          {@code object}.
     * @param {!Object<?,?>} flattened The flattened object being built.
     * @param {string} path The current flattened path.
     * @param {!Object<?,?>} object The nested dict to be flattened.
     */
    function flattenObjectRec(visited, flattened, path, object) {
      if (typeof object !== 'object' || visited.has(object)) {
        return;
      }
      visited.add(object);
      for (const [key, value] of Object.entries(object)) {
        const fullPath = path ? `${path}.${key}` : key;
        // Recurse on non-null objects.
        if (!!value && typeof value === 'object') {
          flattenObjectRec(
              visited, flattened, fullPath,
              /** @type {!Object<?,?>} */ (value));
        } else {
          // Everything else is considered a leaf value.
          flattened[fullPath] = value;
        }
      }
    }

    /**
     * Recursively flattens an Object of key/value pairs. Nested objects will be
     * flattened to paths with a . separator between each key. If there are
     * circular dependencies, they will not be expanded.
     *
     * For example, converting:
     *
     * {
     *   'foo': 'hello',
     *   'bar': 1,
     *   'baz': {
     *     'x': 43.5,
     *     'y': 'fox'
     *     'z': [1, 2]
     *   },
     *   'self': (reference to self)
     * }
     *
     * will yield:
     *
     * {
     *   'foo': 'hello',
     *   'bar': 1,
     *   'baz.x': 43.5,
     *   'baz.y': 'fox',
     *   'baz.z.0': '1',
     *   'baz.y.1': '2'
     * }
     * @param {!Object<?,?>} object The object to be flattened.
     * @return {!Object<?,?>} the flattened object.
     */
    function flattenObject(object) {
      const flattened = {};
      flattenObjectRec(new Set(), flattened, '', object);
      return flattened;
    }

    // The JSON is a dictionary of data describer name to their data. Assuming a
    // convention that describers emit a dictionary from string->string, this is
    // flattened to an array. Each top-level dictionary entry is flattened to a
    // 'heading' with [`the describer's name`, null], followed by some number of
    // entries with a two-element list, each representing a key/value pair.
    this.description_json_ = description_json;
    const description =
        /** @type {!Object<?,?>} */ (JSON.parse(description_json));
    const flattenedDescription = [];
    for (const [title, value] of Object.entries(description)) {
      flattenedDescription.push([title, null]);
      const flattenedValue = flattenObject(value);
      for (const [propName, propValue] of Object.entries(flattenedValue)) {
        let strValue = String(propValue);
        if (strValue.length > 50) {
          strValue = `${strValue.substring(0, 47)}...`;
        }
        flattenedDescription.push([propName, strValue]);
      }
    }
    if (flattenedDescription.length === 0) {
      flattenedDescription.push(['No Data', null]);
    }

    let tr =
        this.div_.selectAll('tbody').selectAll('tr').data(flattenedDescription);
    tr.enter().append('tr').selectAll('td').data(d => d).enter().append('td');
    tr.exit().remove();

    tr = this.div_.selectAll('tr');
    tr.select('td').attr('colspan', function(d) {
      return (d3.select(this.parentElement).datum()[1] === null) ? 2 : null;
    });
    tr = tr.attr('class', d => d[1] === null ? 'heading' : 'value');
    tr.selectAll('td').data(d => d).text(d => d === null ? '' : d);
  }

  /** @private */
  onDragStart_() {
    this.floating = false;
  }

  /** @private */
  onDrag_() {
    this.x = d3.event.x;
    this.y = d3.event.y;
    this.div_.style('left', `${this.x}px`).style('top', `${this.y}px`);

    graph.updateToolTipLinks();
  }
}

/** @implements {d3.ForceNode} */
class GraphNode {
  constructor(id) {
    /** @type {number} */
    this.id = id;
    /** @type {string} */
    this.color = 'black';
    /** @type {string} */
    this.iconUrl = '';

    /** @type {ToolTip} */
    this.tooltip = null;

    /**
     * Implementation of the d3.ForceNode interface.
     * See https://github.com/d3/d3-force#simulation_nodes.
     * @type {number|undefined}
     */
    this.index;
    /** @type {number} */
    this.x;
    /** @type {number} */
    this.y;
    /** @type {number|undefined} */
    this.vx;
    /** @type {number|undefined} */
    this.vy;
    this.fx = null;
    this.fy = null;
  }

  /** @return {string} */
  get title() {
    return '';
  }

  /**
   * Sets the initial x and y position of this node, also resets
   * vx and vy.
   * @param {number} graphWidth Width of the graph view (svg).
   * @param {number} graphHeight Height of the graph view (svg).
   */
  setInitialPosition(graphWidth, graphHeight) {
    this.x = graphWidth / 2;
    this.y = this.targetYPosition(graphHeight);
    this.vx = 0;
    this.vy = 0;
  }

  /**
   * @param {number} graphHeight Height of the graph view (svg).
   * @return {number}
   */
  targetYPosition(graphHeight) {
    const bounds = this.allowedYRange(graphHeight);
    return (bounds[0] + bounds[1]) / 2;
  }

  /**
   * @return {number} The strength of the force that pulls the node towards
   *                    its target y position.
   */
  targetYPositionStrength() {
    return 0.1;
  }

  /**
   * @param {number} graphHeight Height of the graph view.
   * @return {!Array<number>}
   */
  allowedYRange(graphHeight) {
    // By default, nodes just need to be in bounds of the graph.
    return [0, graphHeight];
  }

  /** @return {number} The strength of the repulsion force with other nodes. */
  manyBodyStrength() {
    return -200;
  }

  /** @return {!Array<number>} */
  linkTargets() {
    return [];
  }

  /**
   * Selects a color string from an id.
   * @param {number} id The id the returned color is selected from.
   * @return {string}
   */
  selectColor(id) {
    return d3.schemeSet3[Math.abs(id) % 12];
  }
}

class PageNode extends GraphNode {
  /** @param {!discards.mojom.PageInfo} page */
  constructor(page) {
    super(page.id);
    /** @type {!discards.mojom.PageInfo} */
    this.page = page;
    this.y = kPageNodesTargetY;
  }

  /** override */
  get title() {
    return this.page.mainFrameUrl.url.length > 0 ? this.page.mainFrameUrl.url :
                                                   'Page';
  }

  /** @override */
  targetYPositionStrength() {
    return 10;
  }

  /** override */
  allowedYRange(graphHeight) {
    return [0, kPageNodesYRange];
  }

  /** override */
  manyBodyStrength() {
    return -600;
  }
}

class FrameNode extends GraphNode {
  /** @param {!discards.mojom.FrameInfo} frame */
  constructor(frame) {
    super(frame.id);
    /** @type {!discards.mojom.FrameInfo} frame */
    this.frame = frame;
    this.color = this.selectColor(frame.processId);
  }

  /** override */
  get title() {
    return this.frame.url.url.length > 0 ? this.frame.url.url : 'Frame';
  }

  /** override */
  targetYPosition(graphHeight) {
    return kFrameNodesTargetY;
  }

  /** override */
  allowedYRange(graphHeight) {
    return [kFrameNodesTopMargin, graphHeight - kFrameNodesBottomMargin];
  }

  /** override */
  linkTargets() {
    // Only link to the page if there isn't a parent frame.
    return [
      this.frame.parentFrameId || this.frame.pageId, this.frame.processId
    ];
  }
}

class ProcessNode extends GraphNode {
  /** @param {!discards.mojom.ProcessInfo} process */
  constructor(process) {
    super(process.id);
    /** @type {!discards.mojom.ProcessInfo} */
    this.process = process;

    this.color = this.selectColor(process.id);
  }

  /** override */
  get title() {
    return `PID: ${this.process.pid.pid}`;
  }

  /** @return {number} */
  targetYPositionStrength() {
    return 10;
  }

  /** override */
  allowedYRange(graphHeight) {
    return [graphHeight - kProcessNodesYRange, graphHeight];
  }

  /** override */
  manyBodyStrength() {
    return -600;
  }
}

class WorkerNode extends GraphNode {
  /** @param {!discards.mojom.WorkerInfo} worker */
  constructor(worker) {
    super(worker.id);
    /** @type {!discards.mojom.WorkerInfo} */
    this.worker = worker;

    this.color = this.selectColor(worker.processId);
  }

  /** override */
  get title() {
    return this.worker.url.url.length > 0 ? this.worker.url.url : 'Worker';
  }

  /** @return {number} */
  targetYPositionStrength() {
    return 10;
  }

  /** override */
  allowedYRange(graphHeight) {
    return [
      graphHeight - kWorkerNodesYRange, graphHeight - kProcessNodesYRange
    ];
  }

  /** override */
  manyBodyStrength() {
    return -600;
  }

  /** override */
  linkTargets() {
    // Link the process, in addition to all the client and child workers.
    return [
      this.worker.processId, ...this.worker.clientFrameIds,
      ...this.worker.clientWorkerIds, ...this.worker.childWorkerIds
    ];
  }
}

/**
 * A force that bounds GraphNodes |allowedYRange| in Y.
 * @param {number} graphHeight
 */
function boundingForce(graphHeight) {
  /** @type {!Array<!GraphNode>} */
  let nodes = [];
  /** @type {!Array<!Array>} */
  let bounds = [];

  /** @param {number} alpha */
  function force(alpha) {
    const n = nodes.length;
    for (let i = 0; i < n; ++i) {
      const bound = bounds[i];
      const node = nodes[i];
      const yOld = node.y;
      const yNew = Math.max(bound[0], Math.min(yOld, bound[1]));
      if (yOld !== yNew) {
        node.y = yNew;
        // Zero the velocity of clamped nodes.
        node.vy = 0;
      }
    }
  }

  /** @param {!Array<!GraphNode>} n */
  force.initialize = function(n) {
    nodes = n;
    bounds = nodes.map(node => node.allowedYRange(graphHeight));
  };

  return force;
}

/**
 * @implements {discards.mojom.GraphChangeStreamInterface}
 */
class Graph {
  /**
   * TODO(siggi): This should be SVGElement, but closure doesn't have externs
   *    for this yet.
   * @param {Element} svg
   * @param {Element} div
   */
  constructor(svg, div) {
    /**
     * TODO(siggi): SVGElement.
     * @private {Element}
     */
    this.svg_ = svg;

    /** @private {Element} */
    this.div_ = div;

    /** @private {boolean} */
    this.wasResized_ = false;

    /** @private {number} */
    this.width_ = 0;
    /** @private {number} */
    this.height_ = 0;

    /** @private {d3.ForceSimulation} */
    this.simulation_ = null;

    /**
     * A selection for the top-level <g> node that contains all tooltip links.
     * @private {d3.selection}
     */
    this.toolTipLinkGroup_ = null;

    /**
     * A selection for the top-level <g> node that contains all separators.
     * @private {d3.selection}
     */
    this.separatorGroup_ = null;

    /**
     * A selection for the top-level <g> node that contains all nodes.
     * @private {d3.selection}
     */
    this.nodeGroup_ = null;

    /**
     * A selection for the top-level <g> node that contains all edges.
     * @private {d3.selection}
     */
    this.linkGroup_ = null;

    /** @private {!Map<number, !GraphNode>} */
    this.nodes_ = new Map();

    /**
     * The links.
     * @private {!Array<!d3.ForceLink>}
     */
    this.links_ = [];

    /**
     * The host window.
     * @private {Window}
     */
    this.hostWindow_ = null;

    /**
     * The interval timer used to poll for node descriptions.
     * @private {number}
     */
    this.pollDescriptionsInterval_ = 0;

    /**
     * The d3.drag instance applied to nodes.
     * @private {?d3.Drag}
     */
    this.drag_ = null;
  }

  initialize() {
    // Set up a message listener to receive the graph data from the WebUI.
    // This is hosted in a webview that is never navigated anywhere else,
    // so these event handlers are never removed.
    window.addEventListener('message', this.onMessage_.bind(this));

    // Set up a resize listener to track the graph on resize.
    window.addEventListener('resize', this.onResize_.bind(this));

    // Create the simulation and set up the permanent forces.
    const simulation = d3.forceSimulation();
    simulation.on('tick', this.onTick_.bind(this));

    const linkForce = d3.forceLink().id(d => d.id);
    simulation.force('link', linkForce);

    // Sets the repulsion force between nodes (positive number is attraction,
    // negative number is repulsion).
    simulation.force(
        'charge',
        d3.forceManyBody().strength(this.getManyBodyStrength_.bind(this)));

    this.simulation_ = simulation;

    // Create the <g> elements that host nodes and links.
    // The link groups are created first so that all links end up behind nodes.
    const svg = d3.select(this.svg_);
    this.toolTipLinkGroup_ = svg.append('g').attr('class', 'toolTipLinks');
    this.linkGroup_ = svg.append('g').attr('class', 'links');
    this.nodeGroup_ = svg.append('g').attr('class', 'nodes');
    this.separatorGroup_ = svg.append('g').attr('class', 'separators');

    const drag = d3.drag();
    drag.clickDistance(4);
    drag.on('start', this.onDragStart_.bind(this));
    drag.on('drag', this.onDrag_.bind(this));
    drag.on('end', this.onDragEnd_.bind(this));
    this.drag_ = drag;
  }

  /** @override */
  frameCreated(frame) {
    this.addNode_(new FrameNode(frame));
  }

  /** @override */
  pageCreated(page) {
    this.addNode_(new PageNode(page));
  }

  /** @override */
  processCreated(process) {
    this.addNode_(new ProcessNode(process));
  }

  /** @override */
  workerCreated(worker) {
    this.addNode_(new WorkerNode(worker));
  }

  /** @override */
  frameChanged(frame) {
    const frameNode = /** @type {!FrameNode} */ (this.nodes_.get(frame.id));
    frameNode.frame = frame;
  }

  /** @override */
  pageChanged(page) {
    const pageNode = /** @type {!PageNode} */ (this.nodes_.get(page.id));
    pageNode.page = page;
  }

  /** @override */
  processChanged(process) {
    const processNode =
        /** @type {!ProcessNode} */ (this.nodes_.get(process.id));
    processNode.process = process;
  }

  /** @override */
  workerChanged(worker) {
    const workerNode =
        /** @type {!WorkerNode} */ (this.nodes_.get(worker.id));

    // Worker node links may change dynamically, so account for that here.
    this.removeNodeLinks_(workerNode);
    workerNode.worker = worker;
    this.addNodeLinks_(workerNode);
  }

  /** @override */
  favIconDataAvailable(iconInfo) {
    const graphNode = this.nodes_.get(iconInfo.nodeId);
    if (graphNode) {
      graphNode.iconUrl = 'data:image/png;base64,' + iconInfo.iconData;
    }
  }

  /** @override */
  nodeDeleted(nodeId) {
    const node = this.nodes_.get(nodeId);

    // Remove any links, and then the node itself.
    this.removeNodeLinks_(node);
    this.nodes_.delete(nodeId);
  }

  /** Updates floating tooltip positions as well as links to pinned tooltips */
  updateToolTipLinks() {
    const pinnedTooltips = [];
    for (const node of this.nodes_.values()) {
      const tooltip = node.tooltip;

      if (tooltip) {
        if (tooltip.floating) {
          tooltip.nodeMoved();
        } else {
          pinnedTooltips.push(tooltip);
        }
      }
    }

    function setLineEndpoints(d) {
      const line = d3.select(this);
      const center = d.getCenter();
      line.attr('x1', d => center[0])
          .attr('y1', d => center[1])
          .attr('x2', d => d.node.x)
          .attr('y2', d => d.node.y);
    }

    const toolTipLinks =
        this.toolTipLinkGroup_.selectAll('line').data(pinnedTooltips);
    toolTipLinks.enter()
        .append('line')
        .attr('stroke', 'LightGray')
        .attr('stroke-dasharray', '1')
        .attr('stroke-opacity', '0.8')
        .each(setLineEndpoints);
    toolTipLinks.each(setLineEndpoints);
    toolTipLinks.exit().remove();
  }

  /**
   * @param {!GraphNode} node
   * @private
   */
  removeNodeLinks_(node) {
    // Filter away any links to or from the deleted node.
    this.links_ = this.links_.filter(
        link => link.source !== node && link.target !== node);
  }

  /**
   * @param {!Object<string>} nodeDescriptions
   * @private
   */
  nodeDescriptions_(nodeDescriptions) {
    for (const nodeId in nodeDescriptions) {
      const node = this.nodes_.get(Number.parseInt(nodeId, 10));
      if (node && node.tooltip) {
        node.tooltip.onDescription(nodeDescriptions[nodeId]);
      }
    }
  }

  /**
   * @private
   */
  pollForNodeDescriptions_() {
    const nodeIds = [];
    for (const node of this.nodes_.values()) {
      if (node.tooltip) {
        nodeIds.push(node.id);
      }
    }

    if (nodeIds.length) {
      this.hostWindow_.postMessage(['requestNodeDescriptions', nodeIds], '*');

      if (this.pollDescriptionsInterval_ === 0) {
        // Start polling if not already in progress.
        this.pollDescriptionsInterval_ =
            setInterval(this.pollForNodeDescriptions_.bind(this), 1000);
      }
    } else {
      // No tooltips, stop polling.
      clearInterval(this.pollDescriptionsInterval_);
      this.pollDescriptionsInterval_ = 0;
    }
  }

  /**
   * @param {!Event} event A graph update event posted from the WebUI.
   * @private
   */
  onMessage_(event) {
    if (!this.hostWindow_) {
      this.hostWindow_ = event.source;
    }

    const type = /** @type {string} */ (event.data[0]);
    const data = /** @type {Object|number} */ (event.data[1]);
    switch (type) {
      case 'frameCreated':
        this.frameCreated(
            /** @type {!discards.mojom.FrameInfo} */ (data));
        break;
      case 'pageCreated':
        this.pageCreated(
            /** @type {!discards.mojom.PageInfo} */ (data));
        break;
      case 'processCreated':
        this.processCreated(
            /** @type {!discards.mojom.ProcessInfo} */ (data));
        break;
      case 'workerCreated':
        this.workerCreated(
            /** @type {!discards.mojom.WorkerInfo} */ (data));
        break;
      case 'frameChanged':
        this.frameChanged(
            /** @type {!discards.mojom.FrameInfo} */ (data));
        break;
      case 'pageChanged':
        this.pageChanged(
            /** @type {!discards.mojom.PageInfo} */ (data));
        break;
      case 'processChanged':
        this.processChanged(
            /** @type {!discards.mojom.ProcessInfo} */ (data));
        break;
      case 'favIconDataAvailable':
        this.favIconDataAvailable(
            /** @type {!discards.mojom.FavIconInfo} */ (data));
        break;
      case 'workerChanged':
        this.workerChanged(
            /** @type {!discards.mojom.WorkerInfo} */ (data));
        break;
      case 'nodeDeleted':
        this.nodeDeleted(/** @type {number} */ (data));
        break;
      case 'nodeDescriptions':
        this.nodeDescriptions_(/** @type {!Object<string>} */ (data));
        break;
    }

    this.render_();
  }

  /**
   * @param {GraphNode} node
   * @private
   */
  onGraphNodeClick_(node) {
    if (node.tooltip) {
      node.tooltip.goAway();
      node.tooltip = null;
    } else {
      node.tooltip = new ToolTip(this.div_, node);

      // Poll for all tooltip node descriptions immediately.
      this.pollForNodeDescriptions_();
    }
  }

  /**
   * Renders nodes_ and edges_ to the SVG DOM.
   *
   * Each edge is a line element.
   * Each node is represented as a group element with three children:
   *   1. A circle that has a color and which animates the node on creation
   *      and deletion.
   *   2. An image that is provided a data URL for the nodes favicon, when
   *      available.
   *   3. A title element that presents the nodes URL on hover-over, if
   *      available.
   * Deleted nodes are classed '.dead', and CSS takes care of hiding their
   * image element if it's been populated with an icon.
   *
   * @private
   */
  render_() {
    // Select the links.
    const link = this.linkGroup_.selectAll('line').data(this.links_);
    // Add new links.
    link.enter().append('line').attr('stroke-width', 1);
    // Remove dead links.
    link.exit().remove();

    // Select the nodes, except for any dead ones that are still transitioning.
    const nodes = Array.from(this.nodes_.values());
    const node =
        this.nodeGroup_.selectAll('g:not(.dead)').data(nodes, d => d.id);

    // Add new nodes, if any.
    if (!node.enter().empty()) {
      const newNodes = node.enter()
                           .append('g')
                           .call(this.drag_)
                           .on('click', this.onGraphNodeClick_.bind(this));
      const circles = newNodes.append('circle').attr('r', 9).attr(
          'fill', 'green');  // New nodes appear green.

      newNodes.append('image')
          .attr('x', -8)
          .attr('y', -8)
          .attr('width', 16)
          .attr('height', 16);
      newNodes.append('title');

      // Transition new nodes to their chosen color in 2 seconds.
      circles.transition()
          .duration(2000)
          .attr('fill', d => d.color)
          .attr('r', 6);
    }

    if (!node.exit().empty()) {
      // Give dead nodes a distinguishing class to exclude them from the
      // selection above.
      const deletedNodes = node.exit().classed('dead', true);

      // Interrupt any ongoing transitions.
      deletedNodes.interrupt();

      // Turn down the node associated tooltips.
      deletedNodes.each(d => {
        if (d.tooltip) {
          d.tooltip.goAway();
        }
      });

      // Transition the nodes out and remove them at the end of transition.
      deletedNodes.transition()
          .remove()
          .select('circle')
          .attr('r', 9)
          .attr('fill', 'red')
          .transition()
          .duration(2000)
          .attr('r', 0);
    }

    // Update the title for all nodes.
    node.selectAll('title').text(d => d.title);
    // Update the favicon for all nodes.
    node.selectAll('image').attr('href', d => d.iconUrl);

    // Update and restart the simulation if the graph changed.
    if (!node.enter().empty() || !node.exit().empty() ||
        !link.enter().empty() || !link.exit().empty()) {
      this.simulation_.nodes(nodes);
      this.simulation_.force('link').links(this.links_);

      this.restartSimulation_();
    }
  }

  /** @private */
  onTick_() {
    const nodes = this.nodeGroup_.selectAll('g');
    nodes.attr('transform', d => `translate(${d.x},${d.y})`);

    const lines = this.linkGroup_.selectAll('line');
    lines.attr('x1', d => d.source.x)
        .attr('y1', d => d.source.y)
        .attr('x2', d => d.target.x)
        .attr('y2', d => d.target.y);

    this.updateToolTipLinks();
  }

  /**
   * Adds a new node to the graph, populates its links and gives it an initial
   * position.
   *
   * @param {!GraphNode} node
   * @private
   */
  addNode_(node) {
    this.nodes_.set(node.id, node);
    this.addNodeLinks_(node);
    node.setInitialPosition(this.width_, this.height_);
  }

  /**
   * Adds all the links for a node to the graph.
   *
   * @param {!GraphNode} node
   * @private
   */
  addNodeLinks_(node) {
    const linkTargets = node.linkTargets();
    for (const linkTarget of linkTargets) {
      const target = this.nodes_.get(linkTarget);
      if (target) {
        this.links_.push({source: node, target: target});
      }
    }
  }

  /**
   * @param {!GraphNode} d The dragged node.
   * @private
   */
  onDragStart_(d) {
    if (!d3.event.active) {
      this.restartSimulation_();
    }
    d.fx = d.x;
    d.fy = d.y;
  }

  /**
   * @param {!GraphNode} d The dragged node.
   * @private
   */
  onDrag_(d) {
    d.fx = d3.event.x;
    d.fy = d3.event.y;
  }

  /**
   * @param {!GraphNode} d The dragged node.
   * @private
   */
  onDragEnd_(d) {
    if (!d3.event.active) {
      this.simulation_.alphaTarget(0);
    }
    d.fx = null;
    d.fy = null;
  }

  /**
   * @param {!d3.ForceNode} d The node to position.
   * @private
   */
  getTargetYPosition_(d) {
    return d.targetYPosition(this.height_);
  }

  /**
   * @param {!d3.ForceNode} d The node to position.
   * @private
   */
  getTargetYPositionStrength_(d) {
    return d.targetYPositionStrength();
  }

  /**
   * @param {!d3.ForceNode} d The node to position.
   * @private
   */
  getManyBodyStrength_(d) {
    return d.manyBodyStrength();
  }

  /**
   * @param {number} graphWidth Width of the graph view (svg).
   * @param {number} graphHeight Height of the graph view (svg).
   * @private
   */
  updateSeparators_(graphWidth, graphHeight) {
    const separators = [
      ['Pages', 'Frame Tree', kPageNodesYRange],
      ['', 'Workers', graphHeight - kWorkerNodesYRange],
      ['', 'Processes', graphHeight - kProcessNodesYRange],
    ];
    const kAboveLabelOffset = -6;
    const kBelowLabelOffset = 14;

    const groups = this.separatorGroup_.selectAll('g').data(separators);
    if (groups.enter()) {
      const group = groups.enter().append('g').attr(
          'transform', d => `translate(0,${d[2]})`);
      group.append('line')
          .attr('x1', 10)
          .attr('y1', 0)
          .attr('x2', graphWidth - 10)
          .attr('y2', 0)
          .attr('stroke', 'black')
          .attr('stroke-dasharray', '4');

      group.each(function(d) {
        const parentGroup = d3.select(this);
        if (d[0]) {
          parentGroup.append('text')
              .attr('x', 20)
              .attr('y', kAboveLabelOffset)
              .attr('class', 'separator')
              .text(d => d[0]);
        }
        if (d[1]) {
          parentGroup.append('text')
              .attr('x', 20)
              .attr('y', kBelowLabelOffset)
              .attr('class', 'separator')
              .text(d => d[1]);
        }
      });
    }

    groups.attr('transform', d => `translate(0,${d[2]})`);
    groups.selectAll('line').attr('x2', graphWidth - 10);
  }

  /** @private */
  restartSimulation_() {
    // Restart the simulation.
    this.simulation_.alphaTarget(0.3).restart();
  }

  /**
   * Resizes and restarts the animation after a size change.
   * @private
   */
  onResize_() {
    this.width_ = this.svg_.clientWidth;
    this.height_ = this.svg_.clientHeight;

    this.updateSeparators_(this.width_, this.height_);

    // Reset both X and Y attractive forces, as they're cached.
    const xForce = d3.forceX().x(this.width_ / 2).strength(0.1);
    const yForce = d3.forceY()
                       .y(this.getTargetYPosition_.bind(this))
                       .strength(this.getTargetYPositionStrength_.bind(this));
    this.simulation_.force('x_pos', xForce);
    this.simulation_.force('y_pos', yForce);
    this.simulation_.force('y_bound', boundingForce(this.height_));

    if (!this.wasResized_) {
      this.wasResized_ = true;

      // Reinitialize all node positions on first resize.
      this.nodes_.forEach(
          node => node.setInitialPosition(this.width_, this.height_));

      // Allow the simulation to settle by running it for a bit.
      for (let i = 0; i < 200; ++i) {
        this.simulation_.tick();
      }
    }

    this.restartSimulation_();
  }
}
/* @type {?Graph} */
let graph = null;
function onLoad() {
  graph =
      new Graph(document.querySelector('svg'), document.querySelector('div'));

  graph.initialize();
}

window.addEventListener('load', onLoad);
