// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('model_settings_policy_test', function() {
  suite('ModelSettingsPolicyTest', function() {
    let model = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      model.documentSettings = {
        hasCssMediaStyles: false,
        hasSelection: false,
        isModifiable: true,
        isScalingDisabled: false,
        fitToPageScaling: 100,
        pageCount: 3,
        title: 'title',
      };

      model.pageSize = new print_preview.Size(612, 792);
      model.margins = new print_preview.Margins(72, 72, 72, 72);

      // Create a test destination.
      model.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      model.set(
          'destination.capabilities',
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities);
    });

    test('color managed', function() {
      // Remove color capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.color;

      [{
        // Policy has no effect, setting unavailable
        colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
        colorPolicy: print_preview.ColorMode.COLOR,
        colorDefault: print_preview.ColorMode.COLOR,
        expectedValue: true,
        expectedAvailable: false,
        expectedManaged: false,
        expectedEnforced: true,
      },
       {
         // Policy contradicts actual capabilities, setting unavailable.
         colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
         colorPolicy: print_preview.ColorMode.GRAY,
         colorDefault: print_preview.ColorMode.GRAY,
         expectedValue: true,
         expectedAvailable: false,
         expectedManaged: false,
         expectedEnforced: true,
       },
       {
         // Policy overrides default.
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME', is_default: true},
             {type: 'STANDARD_COLOR'}
           ]
         },
         colorPolicy: print_preview.ColorMode.COLOR,
         // Default mismatches restriction and is ignored.
         colorDefault: print_preview.ColorMode.GRAY,
         expectedValue: true,
         expectedAvailable: true,
         expectedManaged: true,
         expectedEnforced: true,
       },
       {
         // Default defined by policy but setting is modifiable.
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME', is_default: true},
             {type: 'STANDARD_COLOR'}
           ]
         },
         colorDefault: print_preview.ColorMode.COLOR,
         expectedValue: true,
         expectedAvailable: true,
         expectedManaged: false,
         expectedEnforced: false,
       }].forEach(subtestParams => {
        capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        capabilities.printer.color = subtestParams.colorCap;
        const policies = {
          allowedColorModes: subtestParams.colorPolicy,
          defaultColorMode: subtestParams.colorDefault,
        };
        // In practice |capabilities| are always set after |policies| and
        // observers only check for |capabilities|, so the order is important.
        model.set('destination.policies', policies);
        model.set('destination.capabilities', capabilities);
        model.applyDestinationSpecificPolicies();
        assertEquals(
            subtestParams.expectedValue, model.getSettingValue('color'));
        assertEquals(
            subtestParams.expectedAvailable, model.settings.color.available);
        assertEquals(subtestParams.expectedManaged, model.controlsManaged);
        assertEquals(
            subtestParams.expectedEnforced, model.settings.color.setByPolicy);
      });
    });

    test('duplex managed', function() {
      // Remove duplex capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.duplex;

      [{
        // Policy has no effect.
        duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
        duplexPolicy: print_preview.DuplexModeRestriction.SIMPLEX,
        duplexDefault: print_preview.DuplexModeRestriction.SIMPLEX,
        expectedValue: false,
        expectedAvailable: false,
        expectedManaged: false,
        expectedEnforced: true,
      },
       {
         // Policy contradicts actual capabilities and is ignored.
         duplexCap: {option: [{type: 'NO_DUPLEX', is_default: true}]},
         duplexPolicy: print_preview.DuplexModeRestriction.DUPLEX,
         duplexDefault: print_preview.DuplexModeRestriction.LONG_EDGE,
         expectedValue: false,
         expectedAvailable: false,
         expectedManaged: false,
         expectedEnforced: true,
       },
       {
         // Policy overrides default.
         duplexCap: {
           option: [
             {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
             {type: 'SHORT_EDGE'}
           ]
         },
         duplexPolicy: print_preview.DuplexModeRestriction.DUPLEX,
         // Default mismatches restriction and is ignored.
         duplexDefault: print_preview.DuplexModeRestriction.SIMPLEX,
         expectedValue: true,
         expectedAvailable: true,
         expectedManaged: true,
         expectedEnforced: true,
       },
       {
         // Default defined by policy but setting is modifiable.
         duplexCap: {
           option: [
             {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
             {type: 'SHORT_EDGE'}
           ]
         },
         duplexDefault: print_preview.DuplexModeRestriction.LONG_EDGE,
         expectedValue: true,
         expectedAvailable: true,
         expectedManaged: false,
         expectedEnforced: false,
       }].forEach(subtestParams => {
        capabilities =
            print_preview_test_utils.getCddTemplate('FooPrinter').capabilities;
        capabilities.printer.duplex = subtestParams.duplexCap;
        const policies = {
          allowedDuplexModes: subtestParams.duplexPolicy,
          defaultDuplexMode: subtestParams.duplexDefault,
        };
        // In practice |capabilities| are always set after |policies| and
        // observers only check for |capabilities|, so the order is important.
        model.set('destination.policies', policies);
        model.set('destination.capabilities', capabilities);
        model.applyDestinationSpecificPolicies();
        assertEquals(
            subtestParams.expectedValue, model.getSettingValue('duplex'));
        assertEquals(
            subtestParams.expectedAvailable, model.settings.duplex.available);
        assertEquals(subtestParams.expectedManaged, model.controlsManaged);
        assertEquals(
            subtestParams.expectedEnforced, model.settings.duplex.setByPolicy);
      });
    });
  });
});
