Mus+ash
----------
Ash is transitioning from using aura to using mus. During the
transition period ash has support for both aura and mus. In order to
work with both toolkits ash has a porting layer. This layer exists in
ash/common. As portions of ash are converted to the porting layer they
should move to ash/common. DEPS files may also be used to ensure new
dependencies do not get added.
