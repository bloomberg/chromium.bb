define([ 'sprites/Transform' ], function (Transform) {
    return function scale(frameIndex, objectIndex) {
        var scale = (Math.sin((objectIndex + frameIndex * (objectIndex + 1)) / 100) / 2 + 1) * 0.4;
        var x = Math.cos(objectIndex) * 200 + 100;
        var y = Math.sin(objectIndex) * 200 + 100;

        return new Transform({
            x: x,
            y: y,
            scaleX: scale,
            scaleY: scale
        });
    };
});
