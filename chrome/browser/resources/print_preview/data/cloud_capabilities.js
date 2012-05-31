// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Capabilities of a cloud-based print destination.
   * @param {print_preview.CollateCapability} collateCapability Print
   *     destination collate capability.
   * @param {print_preview.ColorCapability} colorCapability Print destination
   *     color capability.
   * @param {print_preview.CopiesCapability} copiesCapability Print destination
   *     copies capability.
   * @param {print_preview.DuplexCapability} duplexCapability Print destination
   *     duplexing capability.
   * @constructor
   * @extends {print_preview.ChromiumCapabilities}
   */
  function CloudCapabilities(
      collateCapability, colorCapability, copiesCapability, duplexCapability) {
    print_preview.ChromiumCapabilities.call(
        this,
        !!copiesCapability,
        '1' /*defaultCopiesStr*/,
        !!collateCapability,
        !!collateCapability && collateCapability.isCollateDefault,
        !!duplexCapability,
        !!duplexCapability && duplexCapability.isDuplexDefault,
        true /*hasOrientationCapability*/,
        false /*defaultIsLandscapeEnabled*/,
        !!colorCapability,
        !!colorCapability && colorCapability.isColorDefault);

    /**
     * Print destination collate capability.
     * @type {print_preview.CollateCapability}
     * @private
     */
    this.collateCapability_ = collateCapability;

    /**
     * Print destination color capability.
     * @type {print_preview.ColorCapability}
     * @private
     */
    this.colorCapability_ = colorCapability;

    /**
     * Print destination copies capability.
     * @type {print_preview.CopiesCapability}
     * @private
     */
    this.copiesCapability_ = copiesCapability;

    /**
     * Print destination duplexing capability.
     * @type {print_preview.DuplexCapability}
     * @private
     */
    this.duplexCapability_ = duplexCapability;
  };

  /**
   * Enumeration of the capability formats of cloud-based print destinations.
   * @enum {string}
   */
  CloudCapabilities.Format = {
    HP: 'hp',
    PPD: 'ppd',
    XPS: 'xps'
  };

  CloudCapabilities.prototype = {
    __proto__: print_preview.ChromiumCapabilities.prototype,

    /**
     * @return {print_preview.CollateCapability} The print destination's collate
     *     capability.
     */
    get collateCapability() {
      return this.collateCapability_;
    },

    /**
     * @return {print_preview.CollateCapability} The print destination's color
     *     capability.
     */
    get colorCapability() {
      return this.colorCapability_;
    },

    /**
     * @return {print_preview.CollateCapability} The print destination's copies
     *     capability.
     */
    get copiesCapability() {
      return this.copiesCapability_;
    },

    /**
     * @return {print_preview.CollateCapability} The print destination's
     *     duplexing capability.
     */
    get duplexCapability() {
      return this.duplexCapability_;
    }
  };

  /**
   * A single print capability of a cloud-based print destination.
   * @param {string} id Identifier of the capability.
   * @param {!print_preview.CloudCapability.Type} type Type of the capability.
   * @constructor
   */
  function CloudCapability(id, type) {
    /**
     * Identifier of the capability.
     * @type {string}
     * @private
     */
    this.id_ = id;

    /**
     * Type of the capability.
     * @type {!print_preview.CloudCapability.Type}
     * @private
     */
    this.type_ = type;
  };

  /**
   * Enumeration of the types of cloud-based print capabilities.
   * @enum {string}
   */
  CloudCapability.Type = {
    FEATURE: 'Feature',
    PARAMETER_DEF: 'ParameterDef'
  };

  CloudCapability.prototype = {
    /** @return {string} Identifier of the capability. */
    get id() {
      return this.id_;
    },

    /** @return {!print_preview.CloudCapability.Type} Type of the capability. */
    get type() {
      return this.type_;
    }
  };

  /**
   * Cloud-based collate capability.
   * @param {string} id Identifier of the collate capability.
   * @param {string} collateOption Identifier of the option that enables
   *     collation.
   * @param {string} noCollateOption Identifier of the option that disables
   *     collation.
   * @param {boolean} isCollateDefault Whether collation is enabled by default.
   * @constructor
   * @extends {print_preview.CloudCapability}
   */
  function CollateCapability(
      id, collateOption, noCollateOption, isCollateDefault) {
    CloudCapability.call(this, id, CloudCapability.Type.FEATURE);

    /**
     * Identifier of the option that enables collation.
     * @type {string}
     * @private
     */
    this.collateOption_ = collateOption;

    /**
     * Identifier of the option that disables collation.
     * @type {string}
     * @private
     */
    this.noCollateOption_ = noCollateOption;

    /**
     * Whether collation is enabled by default.
     * @type {boolean}
     * @private
     */
    this.isCollateDefault_ = isCollateDefault;
  };

  /**
   * Mapping of capability formats to an identifier of the collate capability.
   * @type {!Object.<!CloudCapabilities.Format, string>}
   */
  CollateCapability.Id = {};
  CollateCapability.Id[CloudCapabilities.Format.PPD] = 'Collate';
  CollateCapability.Id[CloudCapabilities.Format.XPS] = 'psk:DocumentCollate';

  /**
   * Regular expression that matches a collate option.
   * @type {!RegExp}
   * @const
   */
  CollateCapability.COLLATE_REGEX = /(.*:collated.*|true)/i;

  /**
   * Regular expression that matches a no-collate option.
   * @type {!RegExp}
   * @const
   */
  CollateCapability.NO_COLLATE_REGEX = /(.*:uncollated.*|false)/i;

  CollateCapability.prototype = {
    __proto__: CloudCapability.prototype,

    /** @return {string} Identifier of the option that enables collation. */
    get collateOption() {
      return this.collateOption_;
    },

    /** @return {string} Identifier of the option that disables collation. */
    get noCollateOption() {
      return this.noCollateOption_;
    },

    /** @return {boolean} Whether collation is enabled by default. */
    get isCollateDefault() {
      return this.isCollateDefault_;
    }
  };

  /**
   * Cloud-based color print capability.
   * @param {string} id Identifier of the color capability.
   * @param {string} colorOption Identifier of the color option.
   * @param {string} bwOption Identifier of the black-white option.
   * @param {boolean} Whether color printing is enabled by default.
   * @constructor
   */
  function ColorCapability(id, colorOption, bwOption, isColorDefault) {
    CloudCapability.call(this, id, CloudCapability.Type.FEATURE);

    /**
     * Identifier of the color option.
     * @type {string}
     * @private
     */
    this.colorOption_ = colorOption;

    /**
     * Identifier of the black-white option.
     * @type {string}
     * @private
     */
    this.bwOption_ = bwOption;

    /**
     * Whether to print in color by default.
     * @type {boolean}
     * @private
     */
    this.isColorDefault_ = isColorDefault;
  };

  /**
   * Mapping of capability formats to an identifier of the color capability.
   * @type {!Object.<!CloudCapabilities.Format, string>}
   */
  ColorCapability.Id = {};
  ColorCapability.Id[CloudCapabilities.Format.HP] = 'ns1:Colors';
  ColorCapability.Id[CloudCapabilities.Format.PPD] = 'ColorModel';
  ColorCapability.Id[CloudCapabilities.Format.XPS] = 'psk:PageOutputColor';

  /**
   * Regular expression that matches a color option.
   * @type {!RegExp}
   * @const
   */
  ColorCapability.COLOR_REGEX = /(.*color.*|.*rgb.*|.*cmy.*|true)/i;

  /**
   * Regular expression that matches a black-white option.
   * @type {!RegExp}
   * @const
   */
  ColorCapability.BW_REGEX = /(.*gray.*|.*mono.*|.*black.*|false)/i;

  ColorCapability.prototype = {
    __proto__: CloudCapability.prototype,

    /** @return {string} Identifier of the color option. */
    get colorOption() {
      return this.colorOption_;
    },

    /** @return {string} Identifier of the black-white option. */
    get bwOption() {
      return this.bwOption_;
    },

    /** @return {boolean} Whether to print in color by default. */
    get isColorDefault() {
      return this.isColorDefault_;
    }
  };

  /**
   * Cloud-based copies print capability.
   * @param {string} id Identifier of the copies capability.
   * @constructor
   */
  function CopiesCapability(id) {
    CloudCapability.call(this, id, CloudCapability.Type.PARAMETER_DEF);
  };

  CopiesCapability.prototype = {
    __proto__: CloudCapability.prototype
  };

  /**
   * Mapping of capability formats to an identifier of the copies capability.
   * @type {!Object.<!CloudCapabilities.Format, string>}
   */
  CopiesCapability.Id = {};
  CopiesCapability.Id[CloudCapabilities.Format.XPS] =
      'psk:JobCopiesAllDocuments';

  /**
   * Cloud-based duplex print capability.
   * @param {string} id Identifier of the duplex capability.
   * @param {string} simplexOption Identifier of the no-duplexing option.
   * @param {string} longEdgeOption Identifier of the duplex on long edge
   *     option.
   * @param {boolean} Whether duplexing is enabled by default.
   * @constructor
   */
  function DuplexCapability(
      id, simplexOption, longEdgeOption, isDuplexDefault) {
    CloudCapability.call(this, id, CloudCapability.Type.FEATURE);

    /**
     * Identifier of the no-duplexing option.
     * @type {string}
     * @private
     */
    this.simplexOption_ = simplexOption;

    /**
     * Identifier of the duplex on long edge option.
     * @type {string}
     * @private
     */
    this.longEdgeOption_ = longEdgeOption;

    /**
     * Whether duplexing is enabled by default.
     * @type {boolean}
     * @private
     */
    this.isDuplexDefault_ = isDuplexDefault;
  };

  /**
   * Mapping of capability formats to an identifier of the duplex capability.
   * @type {!Object.<!CloudCapabilities.Format, string>}
   */
  DuplexCapability.Id = {};
  DuplexCapability.Id[CloudCapabilities.Format.PPD] = 'Duplex';
  DuplexCapability.Id[CloudCapabilities.Format.XPS] =
      'psk:JobDuplexAllDocumentsContiguously';

  /**
   * Regular expression that matches a no-duplexing option.
   * @type {!RegExp}
   * @const
   */
  DuplexCapability.SIMPLEX_REGEX = /(.*onesided.*|.*none.*)/i;

  /**
   * Regular expression that matches a duplex on long edge option.
   * @type {!RegExp}
   * @const
   */
  DuplexCapability.LONG_EDGE_REGEX = /(.*longedge.*|duplexNoTumble)/i;

  DuplexCapability.prototype = {
    __proto__: CloudCapability.prototype,

    /** @return {string} Identifier of the no-duplexing option. */
    get simplexOption() {
      return this.simplexOption_;
    },

    /** @return {string} Identifier of the duplex on long edge option. */
    get longEdgeOption() {
      return this.longEdgeOption_;
    },

    /** @return {boolean} Whether duplexing is enabled by default. */
    get isDuplexDefault() {
      return this.isDuplexDefault_;
    }
  };

  // Export
  return {
    CloudCapabilities: CloudCapabilities,
    CollateCapability: CollateCapability,
    ColorCapability: ColorCapability,
    CopiesCapability: CopiesCapability,
    DuplexCapability: DuplexCapability
  };
});
