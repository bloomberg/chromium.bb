### Background and Motivation

*--- Replace this line and the text below with content ---*

For all PRs that modify *existing* toolkit capabilities or behaviors:

- Provide a detailed description of the current toolkit behavior/capability you are modifying
- Describe the shortcomings of the current behavior/capability (ie: the "bug" part of a BF or the reason for the refactor)
- Describe "what is happening" (the actual crash location / observed behavior) and "why it's happening" (the conditions that lead to the crash / observed behavior)
- Be sure to describe the root cause of the behavior and not just its observed side effects

For all PRs that introduce *new* toolkit capabilities or behaviors:

- Provide a detailed description of why the new capability is necessary
- Describe what problem the capability is solving for our app developers and/or our users

### Description

*--- Replace this line and the text below with content ---*

For all PRs that modify *existing* toolkit capabilities or behaviors:

- Explain how you are changing the existing behavior and how these changes address the original shortcomings (ie: the "fix" part of a BF or the desired results of the refactor)

For all PRs:

- Describe why the approach you are taking is a good approach and point out any shortcomings/tradeoffs
- Describe whether your change is backwards compatible with existing code and how you validated that
- Go through the [Code Review Checklist](/rplus-ui/bundles-ui/blob/master/CODE_REVIEW_CHECKLIST.md) and mention any deviations from those rules here
- (If applicable) Describe how you measured the performance impact of this change
- (If applicable) Provide links to UX wireframes, RFCs or design documents related to this change
- (If applicable) Provide before/after screenshots demonstrating any visual changes

### Test Plan

*--- Replace this line and the text below with content ---*

- Provide a detailed description of how you tested the changes
- Include instructions for manual tests you performed
- Include a list of test cases you added and unit tests you ran to validate this change
- Provide links to FIDDLEs or sample projects that exercise the modified behavior or new capability
- For more details see the ([Testing](#testing) section)

### Dependencies and Release Plan

*--- Replace this line and the text below with content ---*

This whole section is optional and may be deleted if your change doesn't introduce new dependencies or have any special release concerns.

- (If applicable) Describe any dependencies you are taking on *new* bundles or *new* APIs not yet fully released
- (If applicable) Describe any switches/tunables/BREGs/FTAMs that will control the release of your change
- (If applicable) Describe any release concerns and risks, including dependencies that need to go out ahead of this change

### Tickets

*--- Replace this line and the text below with content ---*

- [ENG2BBKIT-1234](https://jira2.prod.bloomberg.com/browse/ENG2BBKIT-1234)
- {DRQS 12345678<GO>}
