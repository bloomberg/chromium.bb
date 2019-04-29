// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Target y position for page nodes.
const kPageNodesTargetY = 20;

// Range occupied by page nodes at the top of the graph view.
const kPageNodesYRange = 100;

// Range occupied by process nodes at the bottom of the graph view.
const kProcessNodesYRange = 150;

// Target y position for frame nodes.
const kFrameNodesTargetY = kPageNodesYRange + 50;

// Range that frame nodes cannot enter at the top/bottom of the graph view.
const kFrameNodesTopMargin = kPageNodesYRange;
const kFrameNodesBottomMargin = kProcessNodesYRange + 50;

/** @implements {d3.ForceNode} */
class GraphNode {
  constructor(id) {
    /** @type {number} */
    this.id = id;
    /** @type {string} */
    this.color = 'black';

    // Implementation of the d3.ForceNode interface.
    // See https://github.com/d3/d3-force#simulation_nodes.
    this.index = null;
    this.x = null;
    this.y = null;
    this.vx = null;
    this.vy = null;
    this.fx = null;
    this.fy = null;
  }

  /** @return {string} */
  get title() {
    return '';
  }

  /**
   * @param {number} graph_height: Height of the graph view (svg).
   * @return {number}
   */
  targetYPosition(graph_height) {
    return 0;
  }

  /**
   * @return {number}: The strength of the force that pulls the node towards
   *                    its target y position.
   */
  targetYPositionStrength() {
    return 0.1;
  }

  /**
   * @param {number} graph_height: Height of the graph view.
   * @return {!Array<number>}
   */
  allowedYRange(graph_height) {
    // By default, there is no hard constraint on the y position of a node.
    return [-Infinity, Infinity];
  }

  /** @return {number}: The strength of the repulsion force with other nodes. */
  manyBodyStrength() {
    return -200;
  }

  /** @return {!Array<number>} */
  linkTargets() {
    return [];
  }

  /**
   * Selects a color string from an id.
   * @param {number} id: The id the returned color is selected from.
   * @return {string}
   */
  selectColor(id) {
    return d3.schemeSet3[Math.abs(id) % 12];
  }
}

class PageNode extends GraphNode {
  /** @param {!resourceCoordinator.mojom.WebUIPageInfo} page */
  constructor(page) {
    super(page.id);
    /** @type {!resourceCoordinator.mojom.WebUIPageInfo} */
    this.page = page;
    this.y = kPageNodesTargetY;
  }

  /** override */
  get title() {
    return this.page.mainFrameUrl.length > 0 ? this.page.mainFrameUrl : 'Page';
  }

  /** override */
  targetYPosition(graph_height) {
    return kPageNodesTargetY;
  }

  /** @override */
  targetYPositionStrength() {
    return 10;
  }

  /** override */
  allowedYRange(graph_height) {
    return [0, kPageNodesYRange];
  }

  /** override */
  manyBodyStrength() {
    return -600;
  }

  /** override */
  linkTargets() {
    return [this.page.mainFrameId];
  }
}

class FrameNode extends GraphNode {
  /** @param {!resourceCoordinator.mojom.WebUIFrameInfo} frame */
  constructor(frame) {
    super(frame.id);
    /** @type {!resourceCoordinator.mojom.WebUIFrameInfo} frame */
    this.frame = frame;
    this.color = this.selectColor(frame.processId);
  }

  /** override */
  get title() {
    return this.frame.url.length > 0 ? this.frame.url : 'Frame';
  }

  /** override */
  targetYPosition(graph_height) {
    return kFrameNodesTargetY;
  }

  /** override */
  allowedYRange(graph_height) {
    return [kFrameNodesTopMargin, graph_height - kFrameNodesBottomMargin];
  }

  /** override */
  linkTargets() {
    return [this.frame.parentFrameId, this.frame.processId];
  }
}

class ProcessNode extends GraphNode {
  /** @param {!resourceCoordinator.mojom.WebUIProcessInfo} process */
  constructor(process) {
    super(process.id);
    /** @type {!resourceCoordinator.mojom.WebUIProcessInfo} */
    this.process = process;

    this.color = this.selectColor(process.id);
  }

  /** override */
  get title() {
    return `PID: ${this.process.pid.pid}`;
  }

  /** override */
  targetYPosition(graph_height) {
    return graph_height - (kProcessNodesYRange / 2);
  }

  /** @return {number} */
  targetYPositionStrength() {
    return 10;
  }

  /** override */
  allowedYRange(graph_height) {
    return [graph_height - kProcessNodesYRange, graph_height];
  }

  /** override */
  manyBodyStrength() {
    return -600;
  }
}


class Graph {
  /**
   * TODO(siggi): This should be SVGElement, but closure doesn't have externs
   *    for this yet.
   * @param {Element} svg
   */
  constructor(svg) {
    /**
     * TODO(siggi): SVGElement.
     * @private {Element}
     */
    this.svg_ = svg;

    /** @private {number} */
    this.width_ = 100;
    /** @private {number} */
    this.height_ = 100;

    /** @private {d3.ForceSimulation} */
    this.simulation_ = null;

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
  }

  initialize() {
    // Set up a message listener to receive the graph data from the WebUI.
    // This is hosted in a webview that is never navigated anywhere else,
    // so these event handlers are never removed.
    window.addEventListener('message', this.onMessage_.bind(this));

    // Set up a window resize listener to track the graph on resize.
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
    // The link group is created first so that all links end up behind nodes.
    const svg = d3.select(this.svg_);
    this.linkGroup_ = svg.append('g').attr('class', 'links');
    this.nodeGroup_ = svg.append('g').attr('class', 'nodes');
  }

  /**
   * @param {!Event} event A graph update event posted from the WebUI.
   * @private
   */
  onMessage_(event) {
    this.onGraphDump_(event.data);
  }

  /** @private */
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
        this.nodeGroup_.selectAll('circle:not(.dead)').data(nodes, d => d.id);

    // Add new nodes, if any.
    if (!node.enter().empty()) {
      const drag = d3.drag();
      drag.on('start', this.onDragStart_.bind(this));
      drag.on('drag', this.onDrag_.bind(this));
      drag.on('end', this.onDragEnd_.bind(this));

      const circles = node.enter()
                          .append('circle')
                          .attr('r', 9)
                          .attr('fill', 'green')  // New nodes appear green.
                          .call(drag);
      circles.append('title');

      // Transition new nodes to their chosen color in 2 seconds.
      circles.transition()
          .duration(2000)
          .attr('fill', d => d.color)
          .attr('r', 6);
    }

    // Give dead nodes a distinguishing class to exclude them from the selection
    // above. Interrupt any ongoing transitions, then transition them out.
    node.exit()
        .classed('dead', true)
        .interrupt()
        .attr('r', 9)
        .attr('fill', 'red')
        .transition()
        .duration(2000)
        .attr('r', 0)
        .remove();

    // Update the title for all nodes.
    node.selectAll('title').text(d => d.title);

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
    const circles = this.nodeGroup_.selectAll('circle');
    circles.attr('cx', this.getClampedXPosition_.bind(this))
        .attr('cy', this.getClampedYPosition_.bind(this));

    const lines = this.linkGroup_.selectAll('line');
    lines.attr('x1', d => d.source.x)
        .attr('y1', d => d.source.y)
        .attr('x2', d => d.target.x)
        .attr('y2', d => d.target.y);
  }

  /**
   * @param {!Map<number, !GraphNode>} oldNodes
   * @param {resourceCoordinator.mojom.WebUIPageInfo} page
   * @private
   */
  addOrUpdatePage_(oldNodes, page) {
    if (!page) {
      return;
    }
    let node = /** @type {?PageNode} */ (oldNodes.get(page.id));
    if (node) {
      node.page = page;
    } else {
      node = new PageNode(page);
    }

    this.nodes_.set(page.id, node);
  }

  /**
   * @param {!Map<number, !GraphNode>} oldNodes
   * @param {resourceCoordinator.mojom.WebUIFrameInfo} frame
   * @private
   */
  addOrUpdateFrame_(oldNodes, frame) {
    if (!frame) {
      return;
    }
    let node = /** @type {?FrameNode} */ (oldNodes.get(frame.id));
    if (node) {
      node.frame = frame;
    } else {
      node = new FrameNode(frame);
    }

    this.nodes_.set(frame.id, node);
  }

  /**
   * @param {!Map<number, !GraphNode>} oldNodes
   * @param {resourceCoordinator.mojom.WebUIProcessInfo} process
   * @private
   */
  addOrUpdateProcess_(oldNodes, process) {
    if (!process) {
      return;
    }
    let node = /** @type {?ProcessNode} */ (oldNodes.get(process.id));
    if (node) {
      node.process = process;
    } else {
      node = new ProcessNode(process);
    }

    this.nodes_.set(process.id, node);
  }

  /**
   * @param {!GraphNode} source
   * @param {number} dst_id
   * @private
   */
  maybeAddLink_(source, dst_id) {
    const target = this.nodes_.get(dst_id);
    if (target) {
      this.links_.push({source: source, target: target});
    }
  }

  /**
   * @param {resourceCoordinator.mojom.WebUIGraph} graph An updated graph from
   *     the WebUI.
   * @private
   */
  onGraphDump_(graph) {
    // Keep a copy of the current node list, as the new node list will copy
    // existing nodes into it.
    const oldNodes = this.nodes_;
    this.nodes_ = new Map();
    for (const page of graph.pages) {
      this.addOrUpdatePage_(oldNodes, page);
    }
    for (const frame of graph.frames) {
      this.addOrUpdateFrame_(oldNodes, frame);
    }
    for (const process of graph.processes) {
      this.addOrUpdateProcess_(oldNodes, process);
    }


    // Recompute the links, there's no benefit to maintaining the identity
    // of the previous links.
    // TODO(siggi): I'm not sure this is true in general. Edges might cache
    //     their individual strengths, as a case in point.
    this.links_ = [];
    const newNodes = this.nodes_.values();
    for (const node of newNodes) {
      const linkTargets = node.linkTargets();
      for (const linkTarget of linkTargets) {
        this.maybeAddLink_(node, linkTarget);
      }
    }

    // TODO(siggi): this is a good place to do initial positioning of new nodes.
    this.render_();
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
  getClampedYPosition_(d) {
    const range = d.allowedYRange(this.height_);
    d.y = Math.max(range[0], Math.min(d.y, range[1]));
    return d.y;
  }

  /**
   * @param {!d3.ForceNode} d The node to position.
   * @private
   */
  getClampedXPosition_(d) {
    d.x = Math.max(10, Math.min(d.x, this.width_ - 10));
    return d.x;
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

    // Reset both X and Y attractive forces, as they're cached.
    const xForce = d3.forceX().x(this.width_ / 2).strength(0.1);
    const yForce = d3.forceY()
                       .y(this.getTargetYPosition_.bind(this))
                       .strength(this.getTargetYPositionStrength_.bind(this));
    this.simulation_.force('x_pos', xForce);
    this.simulation_.force('y_pos', yForce);

    this.restartSimulation_();
  }
}

let graph = null;
function onLoad() {
  graph = new Graph(document.querySelector('svg'));

  graph.initialize();
}

window.addEventListener('load', onLoad);
