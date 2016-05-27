Mus+ash
----------
Ash is transitioning from using aura to using mus. During the
transition period ash has support for both aura and mus. In order to
work with both toolkits ash has a porting layer. This layer exists in
ash/wm/common. As portions of ash are converted to the porting layer
DEPS files are put in place to ensure aura specific dependencies are
not added back.
